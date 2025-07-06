/**
 * LockFree Object Pool - A high-performance, thread-safe object pool implementation
 * Copyright (©) 2025 Daniel <daniel15042015@gmail.com>
 * Repository: https://github.com/beats-dh/lockfree
 * License: https://github.com/beats-dh/lockfree/blob/main/LICENSE
 * Contributors: https://github.com/beats-dh/lockfree/graphs/contributors
 * Website:
 */

#pragma once

#include "atomic_queue/atomic_queue.h"
#include "lib/thread_pool.hpp"
#include "parallel_hashmap/phmap.h"
#include <span>
#include <chrono>
#include <memory_resource>
#include <expected>
#include <version>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable : 4996)
	#pragma warning(disable : 4324)

	#ifndef LOCKFREE_GNU_ATTRIBUTES_DEFINED
		#define LOCKFREE_GNU_ATTRIBUTES_DEFINED
		#define GNUHOT
		#define GNUCOLD
		#define GNUALWAYSINLINE inline
		#define GNUFLATTEN
		#define GNUNOINLINE __declspec(noinline)
	#endif

	#if _MSC_VER >= 1929
		#define LOCKFREE_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
	#else
		#define LOCKFREE_NO_UNIQUE_ADDRESS
	#endif
#else
	#define GNUHOT [[gnu::hot]]
	#define GNUCOLD [[gnu::cold]]
	#define GNUALWAYSINLINE [[gnu::always_inline]] inline
	#define GNUFLATTEN [[gnu::flatten]]
	#define GNUNOINLINE [[gnu::noinline]]

	#if __has_cpp_attribute(no_unique_address) >= 201803L
		#define LOCKFREE_NO_UNIQUE_ADDRESS [[no_unique_address]]
	#else
		#define LOCKFREE_NO_UNIQUE_ADDRESS
	#endif
#endif

#ifdef __AVX2__
	#include <immintrin.h>
#endif

#ifdef __cpp_lib_hardware_interference_size
constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
constexpr size_t CACHE_LINE_SIZE = 64;
#endif

namespace lockfree_config {
	using namespace std::chrono_literals;
	constexpr size_t DEFAULT_POOL_SIZE = 1024;
	constexpr size_t DEFAULT_LOCAL_CACHE_SIZE = 32;
	constexpr size_t PREWARM_BATCH_SIZE = 32;
	constexpr size_t CLEANUP_BATCH_SIZE = 64;
}

template <typename T, typename... Args>
concept HasReset = requires(T t, Args &&... args) {
	{ t.reset(std::forward<Args>(args)...) } -> std::same_as<void>;
};

template <typename T, typename... Args>
concept HasBuild = requires(T t, Args &&... args) {
	{ t.build(std::forward<Args>(args)...) } -> std::same_as<void>;
};

template <typename T>
concept HasDestroy = requires(T t) {
	{ t.destroy() } -> std::same_as<void>;
};

template <typename T>
concept HasThreadId = requires(T t) {
	{ static_cast<const decltype(t) &>(t).threadId } -> std::convertible_to<int16_t>;
};

template <typename T>
concept Poolable = std::is_default_constructible_v<T> || HasReset<T> || HasBuild<T>;

enum class PoolError {
    Shutdown,
    AllocationFailed
};

template <
	typename T,
	size_t PoolSize = lockfree_config::DEFAULT_POOL_SIZE,
	bool EnableStats = false,
	typename Allocator = std::pmr::polymorphic_allocator<T>,
	size_t LocalCacheSize = lockfree_config::DEFAULT_LOCAL_CACHE_SIZE>
class OptimizedObjectPool {
public:
	using pointer = T*;
    using PoolResult = std::expected<pointer, PoolError>;

	struct alignas(CACHE_LINE_SIZE) PoolStatistics {
		size_t acquires = 0, releases = 0, creates = 0, cross_thread_ops = 0,
			   same_thread_hits = 0, in_use = 0, current_pool_size = 0,
			   cache_hits = 0, batch_operations = 0;
	};

	OptimizedObjectPool() : OptimizedObjectPool(Allocator(std::pmr::get_default_resource())) {}

	explicit OptimizedObjectPool(const Allocator &alloc) :
		m_allocator(alloc), m_shutdown_flag(false), m_queue() {
		get_active_instances().emplace(this, std::chrono::steady_clock::now());
		if constexpr (std::is_default_constructible_v<T>) {
			prewarm(PoolSize / 2uz);
		}
	}

	~OptimizedObjectPool() {
		m_shutdown_flag.store(true, std::memory_order_release);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		cleanup_all_caches();
		get_active_instances().erase(this);
		cleanup_global_queue();
	}

	bool safe_return_to_global(pointer obj) noexcept {
		if (m_shutdown_flag.load(std::memory_order_acquire)) [[unlikely]] return false;
		bool result = m_queue.try_push(obj);
		if (result && m_shutdown_flag.load(std::memory_order_relaxed)) [[unlikely]] {
			pointer temp;
			if (m_queue.try_pop(temp) && temp == obj) return false;
		}
		return result;
	}

	void safe_destroy_and_deallocate(pointer obj) noexcept {
		if (!obj) [[unlikely]] return;
		try {
			if constexpr (!std::is_trivially_destructible_v<T>) std::destroy_at(obj);
			m_allocator.deallocate(obj, 1);
		} catch (...) {}
	}

	void batch_return_to_global(std::span<pointer> objects) noexcept {
		if (m_shutdown_flag.load(std::memory_order_acquire)) [[unlikely]] {
			for (auto obj : objects) safe_destroy_and_deallocate(obj);
			return;
		}
		if constexpr (EnableStats) m_stats.batch_operations.fetch_add(1, std::memory_order_relaxed);
		for (auto obj : objects) {
			if (!safe_return_to_global(obj)) safe_destroy_and_deallocate(obj);
		}
	}

	template <typename... Args>
	[[nodiscard]] GNUHOT PoolResult acquire(Args &&... args) {
		if (m_shutdown_flag.load(std::memory_order_acquire)) [[unlikely]] return std::unexpected(PoolError::Shutdown);

		if constexpr (EnableStats) {
			m_stats.acquires.fetch_add(1, std::memory_order_relaxed);
			m_stats.in_use.fetch_add(1, std::memory_order_relaxed);
		}

		auto &cache = local_cache();
		if (cache.size > 0uz) [[likely]] {
			if (cache.is_valid()) [[likely]] {
				pointer obj = cache.data[--cache.size];
				if constexpr (EnableStats) {
					m_stats.same_thread_hits.fetch_add(1, std::memory_order_relaxed);
					m_stats.cache_hits.fetch_add(1, std::memory_order_relaxed);
				}
				construct_or_reset(obj, std::forward<Args>(args)...);
				return obj;
			}
		}

		pointer obj = nullptr;
		if (m_queue.try_pop(obj)) [[likely]] {
			if constexpr (EnableStats) m_stats.cross_thread_ops.fetch_add(1, std::memory_order_relaxed);
			construct_or_reset(obj, std::forward<Args>(args)...);
			return obj;
		}
		return create_new(std::forward<Args>(args)...);
	}

    // CORRIGIDO: Revertido para a sintaxe padrão C++17/20 para compatibilidade com GCC
	GNUHOT void release(pointer obj) noexcept {
		if (!obj) [[unlikely]] return;

		if constexpr (EnableStats) {
			this->m_stats.releases.fetch_add(1, std::memory_order_relaxed);
			this->m_stats.in_use.fetch_sub(1, std::memory_order_relaxed);
		}

		bool same_thread = true;
		if constexpr (HasThreadId<T>) same_thread = obj->threadId == ThreadPool::getThreadId();

		this->cleanup_object_optimized(obj);

		if (same_thread) [[likely]] {
			if (!this->m_shutdown_flag.load(std::memory_order_relaxed)) [[likely]] {
				auto &cache = this->local_cache();
				if (cache.is_valid() && cache.size < LocalCacheSize) [[likely]] {
					cache.data[cache.size++] = obj;
					return;
				}
			}
		}

		if (!this->safe_return_to_global(obj)) {
			this->safe_destroy_and_deallocate(obj);
		}
		if constexpr (EnableStats) {
			if (!same_thread) this->m_stats.cross_thread_ops.fetch_add(1, std::memory_order_relaxed);
		}
	}

	void prewarm(size_t count) {
		if (m_shutdown_flag.load(std::memory_order_acquire)) return;

		count = std::min(count, PoolSize - m_queue.was_size());
		if (count == 0uz) return;

		std::array<pointer, lockfree_config::PREWARM_BATCH_SIZE> batch;
		while (count > 0uz) {
			const size_t n = std::min(count, batch.size());
			size_t allocated = 0;
			for (size_t i = 0; i < n; ++i) {
				if ((batch[i] = allocate_and_construct())) ++allocated;
				else break;
			}
			if (allocated == 0uz) return;
			for (size_t i = 0; i < allocated; ++i) {
				if (!m_queue.try_push(batch[i])) {
					for (size_t j = i; j < allocated; ++j) safe_destroy_and_deallocate(batch[j]);
					return;
				}
			}
			count -= allocated;
		}
	}

	void flush_local_cache() noexcept {
		auto &cache = local_cache();
		if (cache.size > 0uz) {
			batch_return_to_global(std::span(cache.data).first(cache.size));
			cache.size = 0;
		}
	}

	[[nodiscard]] size_t shrink(size_t max = PoolSize) {
		flush_local_cache();
		size_t released = 0;
		constexpr size_t BATCH_SIZE = 16;
		std::array<pointer, BATCH_SIZE> batch;
		while (released < max) {
			size_t batch_count = 0;
			const size_t target = std::min(max - released, BATCH_SIZE);
			for (size_t i = 0; i < target && m_queue.try_pop(batch[i]); ++i) {
				++batch_count;
			}
			if (batch_count == 0uz) break;
			for (auto obj : std::span(batch).first(batch_count)) {
				safe_destroy_and_deallocate(obj);
			}
			released += batch_count;
		}
		return released;
	}

    // CORRIGIDO: Revertido para a sintaxe padrão C++17/20, adicionando 'const'
	[[nodiscard]] PoolStatistics get_stats() const {
		PoolStatistics stats{};
		if constexpr (EnableStats) {
			stats.acquires = this->m_stats.acquires.load(std::memory_order_relaxed);
			stats.releases = this->m_stats.releases.load(std::memory_order_relaxed);
			stats.creates = this->m_stats.creates.load(std::memory_order_relaxed);
			stats.cross_thread_ops = this->m_stats.cross_thread_ops.load(std::memory_order_relaxed);
			stats.same_thread_hits = this->m_stats.same_thread_hits.load(std::memory_order_relaxed);
			stats.in_use = this->m_stats.in_use.load(std::memory_order_relaxed);
			stats.current_pool_size = this->m_queue.was_size();
			stats.cache_hits = this->m_stats.cache_hits.load(std::memory_order_relaxed);
			stats.batch_operations = this->m_stats.batch_operations.load(std::memory_order_relaxed);
		}
		return stats;
	}

	[[nodiscard]] static constexpr size_t capacity() noexcept { return PoolSize; }

private:
	LOCKFREE_NO_UNIQUE_ADDRESS Allocator m_allocator;
	std::atomic<bool> m_shutdown_flag;

	struct alignas(CACHE_LINE_SIZE) StatsBlock {
		std::atomic<size_t> acquires{0}, releases{0}, creates{0}, cross_thread_ops{0},
							same_thread_hits{0}, in_use{0}, cache_hits{0}, batch_operations{0};
	};
	LOCKFREE_NO_UNIQUE_ADDRESS std::conditional_t<EnableStats, StatsBlock, std::monostate> m_stats;

	alignas(CACHE_LINE_SIZE) atomic_queue::AtomicQueue<pointer, PoolSize> m_queue;

	static auto& get_active_instances() {
		static phmap::parallel_flat_hash_map_m<OptimizedObjectPool*, std::chrono::steady_clock::time_point> instances;
		return instances;
	}

	struct alignas(CACHE_LINE_SIZE) ThreadCache {
		size_t size = 0;
		std::atomic<bool> valid{true};
		alignas(CACHE_LINE_SIZE) pointer data[LocalCacheSize];
		bool is_valid() const noexcept { return valid.load(std::memory_order_acquire); }
		void invalidate() noexcept { valid.store(false, std::memory_order_release); }
		~ThreadCache() noexcept;
	};

	static thread_local ThreadCache thread_cache;
	ThreadCache& local_cache() { return thread_cache; }
	static void cleanup_all_caches() { std::this_thread::sleep_for(std::chrono::milliseconds(5)); }

	void cleanup_global_queue() noexcept {
		constexpr size_t BATCH_SIZE = lockfree_config::CLEANUP_BATCH_SIZE;
		std::array<pointer, BATCH_SIZE> batch;
		while (true) {
			size_t batch_count = 0;
			for (size_t i = 0; i < BATCH_SIZE && m_queue.try_pop(batch[i]); ++i) {
				++batch_count;
			}
			if (batch_count == 0uz) break;
			for (auto obj : std::span(batch).first(batch_count)) {
				safe_destroy_and_deallocate(obj);
			}
		}
	}

	template <typename... Args>
	GNUCOLD GNUNOINLINE PoolResult create_new(Args &&... args) {
		if constexpr (EnableStats) m_stats.creates.fetch_add(1, std::memory_order_relaxed);
		pointer obj = nullptr;
		try {
			obj = m_allocator.allocate(1);
			if constexpr (HasBuild<T, Args...>) {
				std::construct_at(obj);
				obj->build(std::forward<Args>(args)...);
			} else {
				std::construct_at(obj, std::forward<Args>(args)...);
			}
			if constexpr (HasThreadId<T>) obj->threadId = ThreadPool::getThreadId();
			return obj;
		} catch (const std::bad_alloc&) {
			if (obj) m_allocator.deallocate(obj, 1);
			if constexpr (EnableStats) m_stats.in_use.fetch_sub(1, std::memory_order_relaxed);
			return std::unexpected(PoolError::AllocationFailed);
		} catch (...) {
			if (obj) m_allocator.deallocate(obj, 1);
			if constexpr (EnableStats) m_stats.in_use.fetch_sub(1, std::memory_order_relaxed);
			throw;
		}
	}

	GNUALWAYSINLINE pointer allocate_and_construct() noexcept {
		try {
			pointer obj = m_allocator.allocate(1);
			if (!obj) return nullptr;
			std::construct_at(obj);
			if constexpr (HasThreadId<T>) obj->threadId = ThreadPool::getThreadId();
			return obj;
		} catch (...) {
			return nullptr;
		}
	}

	GNUHOT GNUALWAYSINLINE void cleanup_object_optimized(pointer obj) noexcept {
		if (!obj) [[unlikely]] return;
		if constexpr (HasReset<T>) {
			try { obj->reset(); } catch (...) {}
		} else if constexpr (HasDestroy<T>) {
			try { obj->destroy(); } catch (...) {}
		}
	}

	template <typename... Args>
	GNUHOT GNUALWAYSINLINE void construct_or_reset(pointer obj, Args &&... args) noexcept {
		if (!obj) [[unlikely]] return;
		if constexpr (HasReset<T, Args...>) {
			try { obj->reset(std::forward<Args>(args)...); } catch (...) {}
		} else if constexpr (HasBuild<T, Args...>) {
			try { obj->build(std::forward<Args>(args)...); } catch (...) {}
		} else if constexpr (!std::is_trivially_destructible_v<T> || sizeof...(Args) > 0) {
			try {
				if constexpr (!std::is_trivially_destructible_v<T>) std::destroy_at(obj);
				std::construct_at(obj, std::forward<Args>(args)...);
			} catch (...) {}
		}
	}
};

template <typename T, size_t P, bool E, typename A, size_t L>
thread_local typename OptimizedObjectPool<T, P, E, A, L>::ThreadCache OptimizedObjectPool<T, P, E, A, L>::thread_cache;

template <typename T, size_t P, bool E, typename A, size_t L>
OptimizedObjectPool<T, P, E, A, L>::ThreadCache::~ThreadCache() noexcept {
    invalidate();
    if (size == 0) return;
    auto& instances = OptimizedObjectPool<T, P, E, A, L>::get_active_instances();
    for (auto obj : std::span(data).first(size)) {
        if (!obj) continue;
        bool returned = false;
        for(auto const& [pool, timestamp] : instances) {
            if (pool && !pool->m_shutdown_flag.load(std::memory_order_acquire)) {
                if (pool->safe_return_to_global(obj)) {
                    returned = true;
                    break;
                }
            }
        }
        if (!returned) {
            try { delete obj; } catch (...) {}
        }
    }
}

template <
    typename T,
    size_t PoolSize = lockfree_config::DEFAULT_POOL_SIZE,
    bool EnableStats = false,
    typename Allocator = std::pmr::polymorphic_allocator<T>,
    size_t LocalCacheSize = lockfree_config::DEFAULT_LOCAL_CACHE_SIZE>
class SharedOptimizedObjectPool {
public:
    using PoolResult = std::expected<std::shared_ptr<T>, PoolError>;

	SharedOptimizedObjectPool() : m_pool(Allocator(std::pmr::get_default_resource())) {}
	explicit SharedOptimizedObjectPool(const Allocator &allocator) : m_pool(allocator) {}

	template <typename... Args>
	[[nodiscard]] PoolResult acquire(Args &&... args) {
		auto result = m_pool.acquire(std::forward<Args>(args)...);
		if (!result) [[unlikely]] return std::unexpected(result.error());
		return std::shared_ptr<T>(result.value(), [this](T* ptr) noexcept {
			if (ptr) m_pool.release(ptr);
		});
	}

	void prewarm(size_t count) { m_pool.prewarm(count); }
	void flush_local_cache() { m_pool.flush_local_cache(); }
	[[nodiscard]] size_t shrink(size_t max = PoolSize) { return m_pool.shrink(max); }
	[[nodiscard]] auto get_stats() const { return m_pool.get_stats(); }
	[[nodiscard]] static constexpr size_t capacity() noexcept { return PoolSize; }

private:
	OptimizedObjectPool<T, PoolSize, EnableStats, Allocator, LocalCacheSize> m_pool;
};

