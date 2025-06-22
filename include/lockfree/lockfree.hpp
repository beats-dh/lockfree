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

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable : 4996)  // Deprecated functions
	#pragma warning(disable : 4324)  // Structure padded due to alignment specifier

	#ifndef LOCKFREE_GNU_ATTRIBUTES_DEFINED
		#define LOCKFREE_GNU_ATTRIBUTES_DEFINED
		#define GNUHOT
		#define GNUCOLD
		#define GNUALWAYSINLINE inline
		#define GNUFLATTEN
		#define GNUNOINLINE __declspec(noinline)
	#endif

	// MSVC compatibility for no_unique_address
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

	// GCC/Clang compatibility for no_unique_address
	#if defined(__has_cpp_attribute)
		#if __has_cpp_attribute(no_unique_address) >= 201803L
			#define LOCKFREE_NO_UNIQUE_ADDRESS [[no_unique_address]]
		#else
			#define LOCKFREE_NO_UNIQUE_ADDRESS
		#endif
	#else
		#if __cplusplus >= 202002L
			#define LOCKFREE_NO_UNIQUE_ADDRESS [[no_unique_address]]
		#else
			#define LOCKFREE_NO_UNIQUE_ADDRESS
		#endif
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
	constexpr size_t DEFAULT_POOL_SIZE = 1024;
	constexpr size_t DEFAULT_LOCAL_CACHE_SIZE = 32;
	constexpr size_t PREWARM_BATCH_SIZE = 32;
	constexpr size_t CLEANUP_BATCH_SIZE = 64;
	constexpr size_t CLEANUP_COUNTER_THRESHOLD = 512;
	constexpr auto TIMESTAMP_CLEANUP_INTERVAL = std::chrono::minutes(5);
	constexpr auto TIMESTAMP_RETENTION_TIME = std::chrono::minutes(1);
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

/**
 * @brief Ultra-fast lock-free object pool with optimized cleanup and thread-local caching
 *
 * This class provides a high-performance, thread-safe object pool implementation that uses
 * lock-free atomic operations and thread-local caches to minimize contention. Objects are
 * recycled efficiently with SIMD-optimized construction and LIFO cache ordering for optimal
 * cache locality.
 *
 * @tparam T Type of objects to pool (must satisfy Poolable concept)
 * @tparam PoolSize Maximum capacity of the global pool (must be power of two)
 * @tparam EnableStats Enable statistics collection for monitoring performance
 * @tparam Allocator Allocator type (supports pmr allocators)
 * @tparam LocalCacheSize Maximum objects per-thread cache for reduced contention
 */
template <
	typename T,
	size_t PoolSize = lockfree_config::DEFAULT_POOL_SIZE,
	bool EnableStats = false,
	typename Allocator = std::pmr::polymorphic_allocator<T>,
	size_t LocalCacheSize = lockfree_config::DEFAULT_LOCAL_CACHE_SIZE>
class OptimizedObjectPool {
public:
	using pointer = T*;

	struct alignas(CACHE_LINE_SIZE) PoolStatistics {
		size_t acquires = 0;
		size_t releases = 0;
		size_t creates = 0;
		size_t cross_thread_ops = 0;
		size_t same_thread_hits = 0;
		size_t in_use = 0;
		size_t current_pool_size = 0;
		size_t cache_hits = 0;
		size_t batch_operations = 0;
	};

	/**
	 * @brief Default constructor using the default memory resource
	 */
	OptimizedObjectPool() :
		OptimizedObjectPool(Allocator { std::pmr::get_default_resource() }) { }

	/**
	 * @brief Construct pool with custom allocator
	 * @param alloc Custom allocator instance for object allocation
	 *
	 * Initializes the pool, registers it for safe cleanup, and optionally prewarms
	 * the pool with default-constructible objects for better initial performance.
	 */
	explicit OptimizedObjectPool(const Allocator &alloc) :
		m_allocator(alloc), m_shutdown_flag(false), m_queue() {
		get_active_instances().emplace(this, std::chrono::steady_clock::now());

		if constexpr (std::is_default_constructible_v<T>) {
			prewarm(PoolSize / 2);
		}
	}

	/**
	 * @brief Destructor with safe multi-threaded cleanup
	 *
	 * Ensures proper shutdown order: signals shutdown, waits for ongoing operations,
	 * cleans thread caches, removes from active instances, and cleans global queue.
	 * This ordering prevents race conditions during pool destruction.
	 */
	~OptimizedObjectPool() {
		m_shutdown_flag.store(true, std::memory_order_release);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		cleanup_all_caches();
		get_active_instances().erase(this);
		cleanup_global_queue();
	}

	/**
	 * @brief Safely return object to global pool with shutdown checks
	 * @param obj Pointer to object being returned
	 * @return true if object was successfully returned to pool, false otherwise
	 *
	 * Attempts to push object to global atomic queue with double-checking for
	 * shutdown state. If pool begins shutdown after push, attempts recovery.
	 */
	bool safe_return_to_global(pointer obj) noexcept {
		if (m_shutdown_flag.load(std::memory_order_acquire)) [[unlikely]] {
			return false;
		}

		bool result = m_queue.try_push(obj);

		if (result && m_shutdown_flag.load(std::memory_order_relaxed)) [[unlikely]] {
			pointer temp;
			if (m_queue.try_pop(temp) && temp == obj) {
				return false;
			}
		}

		return result;
	}

	/**
	 * @brief Safely destroy and deallocate object with exception handling
	 * @param obj Pointer to object to destroy and deallocate
	 *
	 * Performs proper destruction sequence: calls destructor if not trivial,
	 * then deallocates memory. All exceptions are caught and ignored for
	 * safe cleanup in destructors and error paths.
	 */
	void safe_destroy_and_deallocate(pointer obj) noexcept {
		if (!obj) [[unlikely]] {
			return;
		}

		try {
			if constexpr (!std::is_trivially_destructible_v<T>) {
				std::destroy_at(obj);
			}
			m_allocator.deallocate(obj, 1);
		} catch (...) {
		}
	}

	/**
	 * @brief Batch return multiple objects to global pool efficiently
	 * @param objects Span of object pointers to return
	 *
	 * Optimized batch operation that attempts to return all objects to global
	 * pool. If pool is shutting down, destroys all objects instead. Updates
	 * batch operation statistics when enabled.
	 */
	void batch_return_to_global(std::span<pointer> objects) noexcept {
		if (m_shutdown_flag.load(std::memory_order_acquire)) [[unlikely]] {
			for (auto obj : objects) {
				safe_destroy_and_deallocate(obj);
			}
			return;
		}

		if constexpr (EnableStats) {
			m_stats.batch_operations.fetch_add(1, std::memory_order_relaxed);
		}

		for (auto obj : objects) {
			if (!safe_return_to_global(obj)) {
				safe_destroy_and_deallocate(obj);
			}
		}
	}

	/**
	 * @brief Acquire object from pool with optional constructor arguments
	 * @tparam Args Constructor argument types
	 * @param args Arguments to forward to object constructor or reset method
	 * @return Pointer to acquired object, or nullptr on failure
	 *
	 * High-performance acquisition with multi-level caching: checks thread-local
	 * cache first (LIFO for cache locality), then global atomic queue, finally
	 * creates new object. Includes prefetching hints and SIMD-optimized construction.
	 */
	template <typename... Args>
	[[nodiscard]] GNUHOT pointer acquire(Args &&... args) {
		if (m_shutdown_flag.load(std::memory_order_acquire)) [[unlikely]] {
			return create_new(std::forward<Args>(args)...);
		}

		if constexpr (EnableStats) {
			m_stats.acquires.fetch_add(1, std::memory_order_relaxed);
			m_stats.in_use.fetch_add(1, std::memory_order_relaxed);
		}

		auto &cache = local_cache();

		if (cache.size > 0) [[likely]] {
			if (cache.is_valid()) [[likely]] {
				const size_t new_size = cache.size - 1;
				pointer obj = cache.data[new_size];
				cache.size = new_size;

				if constexpr (EnableStats) {
					m_stats.same_thread_hits.fetch_add(1, std::memory_order_relaxed);
					m_stats.cache_hits.fetch_add(1, std::memory_order_relaxed);
				}

				if (new_size > 0) [[likely]] {
					const size_t prefetch_idx = new_size - 1;
#if defined(__GNUC__) || defined(__clang__)
					__builtin_prefetch(cache.data[prefetch_idx], 1, 3);
					if (new_size > 1) {
						__builtin_prefetch(cache.data[prefetch_idx - 1], 1, 2);
					}
#elif defined(_MSC_VER) && defined(_M_X64)
					_mm_prefetch(reinterpret_cast<const char*>(cache.data[prefetch_idx]), _MM_HINT_T0);
#endif
				}

#if defined(__GNUC__) || defined(__clang__)
				__builtin_prefetch(obj, 1, 3);
#elif defined(_MSC_VER) && defined(_M_X64)
				_mm_prefetch(reinterpret_cast<const char*>(obj), _MM_HINT_T0);
#endif

				if constexpr (std::is_trivially_copyable_v<T> && sizeof...(Args) == 0) {
					return obj;
				}

				construct_or_reset(obj, std::forward<Args>(args)...);
				return obj;
			}
		}

		pointer obj = nullptr;
		if (m_queue.try_pop(obj)) [[likely]] {
			if constexpr (EnableStats) {
				m_stats.cross_thread_ops.fetch_add(1, std::memory_order_relaxed);
			}
			construct_or_reset(obj, std::forward<Args>(args)...);
			return obj;
		}

		return create_new(std::forward<Args>(args)...);
	}

	/**
	 * @brief Release object back to pool with thread affinity optimization
	 * @param obj Pointer to object being released
	 *
	 * Optimized release path: cleans object, prefers thread-local cache for
	 * same-thread releases, falls back to global pool for cross-thread operations.
	 * Tracks cross-thread statistics accurately and handles cache overflow.
	 */
	GNUHOT void release(pointer obj) noexcept {
		if (!obj) [[unlikely]] {
			return;
		}

		if constexpr (EnableStats) {
			m_stats.releases.fetch_add(1, std::memory_order_relaxed);
			m_stats.in_use.fetch_sub(1, std::memory_order_relaxed);
		}

		bool same_thread = true;
		if constexpr (HasThreadId<T>) {
			const auto current_thread = ThreadPool::getThreadId();
			same_thread = obj->threadId == current_thread;
		}

		cleanup_object_optimized(obj);

		if (same_thread) [[likely]] {
			if (!m_shutdown_flag.load(std::memory_order_relaxed)) [[likely]] {
				auto &cache = local_cache();
				const size_t current_size = cache.size;

				if (cache.is_valid() && current_size < LocalCacheSize) [[likely]] {
					cache.data[current_size] = obj;
					cache.size = current_size + 1;

					if (current_size > 0) [[likely]] {
#if defined(__GNUC__) || defined(__clang__)
						__builtin_prefetch(cache.data[current_size - 1], 0, 3);
#endif
					}
					return;
				}
			}
		}

		bool cross_thread_operation = !same_thread;
		bool returned_to_global = safe_return_to_global(obj);

		if constexpr (EnableStats) {
			if (cross_thread_operation && returned_to_global) {
				m_stats.cross_thread_ops.fetch_add(1, std::memory_order_relaxed);
			}
		}

		if (!returned_to_global) {
			safe_destroy_and_deallocate(obj);
		}
	}

	/**
	 * @brief Pre-populate pool with ready-to-use objects
	 * @param count Number of objects to create and add to pool
	 *
	 * Batch creates objects and adds them to global pool for improved initial
	 * performance. Uses configurable batch size for memory efficiency and
	 * stops on first allocation failure or pool full condition.
	 */
	void prewarm(size_t count) {
		if (m_shutdown_flag.load(std::memory_order_acquire)) {
			return;
		}

		count = std::min(count, PoolSize - m_queue.was_size());
		if (count == 0) [[unlikely]] {
			return;
		}

		constexpr size_t BATCH_SIZE = lockfree_config::PREWARM_BATCH_SIZE;
		std::array<pointer, BATCH_SIZE> batch;

		while (count > 0) {
			const size_t n = std::min(count, BATCH_SIZE);

			size_t allocated = 0;
			for (size_t i = 0; i < n; ++i) {
				batch[i] = allocate_and_construct();
				if (batch[i]) {
					++allocated;
				} else {
					break;
				}
			}

			if (allocated == 0) [[unlikely]] {
				return;
			}

			for (size_t i = 0; i < allocated; ++i) {
				if (!m_queue.try_push(batch[i])) {
					for (size_t j = i; j < allocated; ++j) {
						safe_destroy_and_deallocate(batch[j]);
					}
					return;
				}
			}

			count -= allocated;
		}
	}

	/**
	 * @brief Flush current thread's local cache to global pool
	 *
	 * Forces return of all cached objects to global pool using batch operation.
	 * Useful for load balancing or before thread termination to prevent
	 * object loss and improve cross-thread object availability.
	 */
	void flush_local_cache() noexcept {
		auto &cache = local_cache();
		if (cache.size > 0) {
			std::span<pointer> objects(cache.data, cache.size);
			batch_return_to_global(objects);
			cache.size = 0;
		}
	}

	/**
	 * @brief Reduce pool size by destroying excess objects
	 * @param max Maximum number of objects to destroy
	 * @return Number of objects actually destroyed
	 *
	 * Shrinks pool by removing objects from global queue in batches.
	 * First flushes local cache, then destroys objects from global pool
	 * up to specified limit. Useful for memory pressure situations.
	 */
	[[nodiscard]] size_t shrink(size_t max = PoolSize) {
		flush_local_cache();

		size_t released = 0;
		constexpr size_t BATCH_SIZE = 16;
		std::array<pointer, BATCH_SIZE> batch;

		while (released < max) {
			size_t batch_count = 0;
			const size_t remaining = max - released;
			const size_t target = std::min(remaining, BATCH_SIZE);

			for (size_t i = 0; i < target && m_queue.try_pop(batch[i]); ++i) {
				++batch_count;
			}

			if (batch_count == 0) {
				break;
			}

			for (size_t i = 0; i < batch_count; ++i) {
				safe_destroy_and_deallocate(batch[i]);
			}

			released += batch_count;
		}

		return released;
	}

	/**
	 * @brief Get current pool performance statistics
	 * @return PoolStatistics struct with current counters
	 *
	 * Returns atomic snapshot of pool performance metrics including
	 * acquisition/release counts, cache hit rates, and cross-thread operations.
	 * Statistics are only collected when EnableStats template parameter is true.
	 */
	[[nodiscard]] PoolStatistics get_stats() const {
		PoolStatistics stats {};
		if constexpr (EnableStats) {
			stats.acquires = m_stats.acquires.load(std::memory_order_relaxed);
			stats.releases = m_stats.releases.load(std::memory_order_relaxed);
			stats.creates = m_stats.creates.load(std::memory_order_relaxed);
			stats.cross_thread_ops = m_stats.cross_thread_ops.load(std::memory_order_relaxed);
			stats.same_thread_hits = m_stats.same_thread_hits.load(std::memory_order_relaxed);
			stats.in_use = m_stats.in_use.load(std::memory_order_relaxed);
			stats.current_pool_size = m_queue.was_size();
			stats.cache_hits = m_stats.cache_hits.load(std::memory_order_relaxed);
			stats.batch_operations = m_stats.batch_operations.load(std::memory_order_relaxed);
		}
		return stats;
	}

	/**
	 * @brief Get compile-time pool capacity
	 * @return Maximum pool size specified at compile time
	 */
	[[nodiscard]] static constexpr size_t capacity() noexcept {
		return PoolSize;
	}

private:
	LOCKFREE_NO_UNIQUE_ADDRESS Allocator m_allocator;
	std::atomic<bool> m_shutdown_flag;

	struct alignas(CACHE_LINE_SIZE) StatsBlock {
		std::atomic<size_t> acquires { 0 };
		std::atomic<size_t> releases { 0 };
		std::atomic<size_t> creates { 0 };
		std::atomic<size_t> cross_thread_ops { 0 };
		std::atomic<size_t> same_thread_hits { 0 };
		std::atomic<size_t> in_use { 0 };
		std::atomic<size_t> cache_hits { 0 };
		std::atomic<size_t> batch_operations { 0 };
	};
	LOCKFREE_NO_UNIQUE_ADDRESS std::conditional_t<EnableStats, StatsBlock, std::monostate> m_stats;

	alignas(CACHE_LINE_SIZE) atomic_queue::AtomicQueue<pointer, PoolSize> m_queue;

	/**
	 * @brief Get reference to active instances map with guaranteed initialization order
	 * @return Reference to thread-safe parallel hash map
	 *
	 * Uses Meyer's singleton pattern to ensure phmap::parallel_flat_hash_map_m is
	 * initialized on-demand, avoiding static initialization order fiasco. The hash map
	 * is thread-safe with internal mutexes and optimized for concurrent access.
	 */
	static auto& get_active_instances() {
		static phmap::parallel_flat_hash_map_m<OptimizedObjectPool*, std::chrono::steady_clock::time_point> instances;
		return instances;
	}

	struct alignas(CACHE_LINE_SIZE) ThreadCache {
		size_t size = 0;
		std::atomic<bool> valid { true };

		// Use empty base optimization when no padding needed
		struct PaddingBlock1 {
			static constexpr size_t METADATA_SIZE = sizeof(size_t) + sizeof(std::atomic<bool>);
			static constexpr size_t REMAINDER = METADATA_SIZE % CACHE_LINE_SIZE;
			static constexpr size_t NEEDED = REMAINDER == 0 ? 0 : CACHE_LINE_SIZE - REMAINDER;

			// Only add padding bytes if actually needed
			std::conditional_t<NEEDED == 0, std::monostate, std::array<std::byte, NEEDED>> padding;
		} padding_block1;

		alignas(CACHE_LINE_SIZE) pointer data[LocalCacheSize];

		struct PaddingBlock2 {
			static constexpr size_t DATA_SIZE = sizeof(pointer) * LocalCacheSize;
			static constexpr size_t REMAINDER = DATA_SIZE % CACHE_LINE_SIZE;
			static constexpr size_t NEEDED = REMAINDER == 0 ? 0 : CACHE_LINE_SIZE - REMAINDER;

			// Only add padding bytes if actually needed
			std::conditional_t<NEEDED == 0, std::monostate, std::array<std::byte, NEEDED>> padding;
		} padding_block2;

		/**
		 * @brief Check if thread cache is valid for operations
		 * @return true if cache can be safely used
		 */
		bool is_valid() const noexcept {
			return valid.load(std::memory_order_acquire);
		}

		/**
		 * @brief Mark thread cache as invalid to prevent further use
		 */
		void invalidate() noexcept {
			valid.store(false, std::memory_order_release);
		}

		/**
		 * @brief Clear cache contents to specified pool using batch operation
		 * @param pool Pool to receive cached objects
		 */
		void clear_to_pool(auto* pool) noexcept {
			if (size == 0) {
				return;
			}

			std::span<pointer> objects(data, size);
			pool->batch_return_to_global(objects);
			size = 0;
		}

		/**
		 * @brief Destructor with safe cross-pool object return
		 *
		 * Attempts to return cached objects to active pools using the singleton
		 * active instances map. Falls back to simple deletion if pools are
		 * unavailable or shutting down. This approach is safe during static
		 * destruction and maximizes object reuse.
		 */
		~ThreadCache() noexcept {
			invalidate();

			std::vector<OptimizedObjectPool*> active_pools;
			try {
				auto& instances = get_active_instances();
				active_pools.reserve(instances.size());
				for (const auto &[pool, timestamp] : instances) {
					active_pools.push_back(pool);
				}
			} catch (...) {
			}

			for (size_t i = 0; i < size; ++i) {
				if (data[i]) {
					bool handled = false;

					for (auto* pool : active_pools) {
						if (pool && !pool->m_shutdown_flag.load(std::memory_order_acquire)) {
							if (pool->safe_return_to_global(data[i])) {
								data[i] = nullptr;
								handled = true;
								break;
							}
						}
					}

					if (!handled && data[i]) {
						try {
							delete data[i];
						} catch (...) {
						}
						data[i] = nullptr;
					}
				}
			}
			size = 0;
		}
	};

	/**
	 * @brief Get reference to current thread's local cache
	 * @return Reference to thread-local ThreadCache instance
	 */
	ThreadCache &local_cache() {
		return thread_cache;
	}

	static thread_local ThreadCache thread_cache;

	/**
	 * @brief Allow natural cleanup of thread caches during shutdown
	 *
	 * Provides small delay for threads to complete ongoing operations
	 * and naturally flush their caches before forced cleanup.
	 */
	static void cleanup_all_caches() {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	/**
	 * @brief Clean remaining objects from global queue during destruction
	 *
	 * Processes global atomic queue in batches to destroy all remaining
	 * objects safely during pool destruction. Uses configurable batch
	 * size for memory efficiency.
	 */
	void cleanup_global_queue() noexcept {
		constexpr size_t CLEANUP_BATCH_SIZE = lockfree_config::CLEANUP_BATCH_SIZE;
		std::array<pointer, CLEANUP_BATCH_SIZE> batch;

		while (true) {
			size_t batch_count = 0;

			for (size_t i = 0; i < CLEANUP_BATCH_SIZE && m_queue.try_pop(batch[i]); ++i) {
				++batch_count;
			}

			if (batch_count == 0) {
				break;
			}

			for (size_t i = 0; i < batch_count; ++i) {
				safe_destroy_and_deallocate(batch[i]);
			}
		}
	}

	/**
	 * @brief Create new object with proper initialization and thread tracking
	 * @tparam Args Constructor argument types
	 * @param args Arguments to forward to object constructor
	 * @return Pointer to newly created object
	 *
	 * Allocates memory, constructs object with provided arguments, and sets
	 * thread ID if supported. Updates creation statistics when enabled.
	 * Provides proper exception safety with cleanup on failure.
	 */
	template <typename... Args>
	GNUCOLD GNUNOINLINE pointer create_new(Args &&... args) {
		if constexpr (EnableStats) {
			m_stats.creates.fetch_add(1, std::memory_order_relaxed);
		}

		pointer obj = nullptr;
		try {
			obj = m_allocator.allocate(1);
			if constexpr (HasBuild<T, Args...>) {
				std::construct_at(obj);
				obj->build(std::forward<Args>(args)...);
			} else {
				std::construct_at(obj, std::forward<Args>(args)...);
			}

			if constexpr (HasThreadId<T>) {
				obj->threadId = ThreadPool::getThreadId();
			}

			return obj;
		} catch (...) {
			if (obj) {
				m_allocator.deallocate(obj, 1);
			}
			if constexpr (EnableStats) {
				m_stats.in_use.fetch_sub(1, std::memory_order_relaxed);
			}
			throw;
		}
	}

	/**
	 * @brief Allocate and default-construct object for pool prewarming
	 * @return Pointer to allocated object, or nullptr on failure
	 *
	 * Simplified allocation for prewarming that only default-constructs
	 * objects. Uses nothrow operations when possible and sets thread ID.
	 * Returns nullptr on any failure rather than throwing exceptions.
	 */
	GNUALWAYSINLINE pointer allocate_and_construct() noexcept {
		pointer obj = nullptr;
		try {
			obj = m_allocator.allocate(1);
			if (!obj) [[unlikely]] {
				return nullptr;
			}

			if constexpr (std::is_nothrow_default_constructible_v<T>) {
				std::construct_at(obj);
			} else {
				new (obj) T {};
			}

			if constexpr (HasThreadId<T>) {
				obj->threadId = ThreadPool::getThreadId();
			}

			return obj;
		} catch (...) {
			if (obj) {
				m_allocator.deallocate(obj, 1);
			}
			return nullptr;
		}
	}

	/**
	 * @brief Clean/reset object for reuse with exception safety
	 * @param obj Pointer to object to clean
	 *
	 * Attempts to reset object state using reset() method if available,
	 * otherwise uses destroy() method. Optimized for common case branch
	 * prediction with reset() as primary path.
	 */
	GNUHOT GNUALWAYSINLINE void cleanup_object_optimized(pointer obj) noexcept {
		if (!obj) [[unlikely]] {
			return;
		}

		if constexpr (HasReset<T>) {
			try {
				obj->reset();
			} catch (...) {
			}
		} else if constexpr (HasDestroy<T>) {
			try {
				obj->destroy();
			} catch (...) {
			}
		}
	}

	/**
	 * @brief Optimally construct or reset object with SIMD optimizations
	 * @tparam Args Constructor argument types
	 * @param obj Pointer to object to initialize
	 * @param args Arguments for construction/reset
	 *
	 * Multi-strategy object initialization: uses reset() for pool efficiency,
	 * build() for complex initialization, SIMD-optimized copying for trivial
	 * types, and full reconstruction for complex types. Template specializations
	 * provide optimal performance for different object sizes.
	 */
	template <typename... Args>
	GNUHOT GNUALWAYSINLINE void construct_or_reset(pointer obj, Args &&... args) {
		if (!obj) [[unlikely]] {
			return;
		}

		if constexpr (std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T> && sizeof...(Args) == 0) {
			return;
		}

		if constexpr (HasReset<T, Args...>) {
			try {
				obj->reset(std::forward<Args>(args)...);
				return;
			} catch (...) {
			}
		}

		if constexpr (HasBuild<T, Args...>) {
			try {
				obj->build(std::forward<Args>(args)...);
				return;
			} catch (...) {
			}
		}

		if constexpr (std::is_trivially_copyable_v<T> && sizeof...(Args) == 1) {
			using FirstArg = std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>;
			if constexpr (std::is_same_v<FirstArg, T>) {
				const T &src = std::get<0>(std::forward_as_tuple(args...));

				if constexpr (sizeof(T) == 1) {
					*reinterpret_cast<uint8_t*>(obj) = *reinterpret_cast<const uint8_t*>(&src);
				} else if constexpr (sizeof(T) == 2) {
					*reinterpret_cast<uint16_t*>(obj) = *reinterpret_cast<const uint16_t*>(&src);
				} else if constexpr (sizeof(T) == 4) {
					*reinterpret_cast<uint32_t*>(obj) = *reinterpret_cast<const uint32_t*>(&src);
				} else if constexpr (sizeof(T) == 8) {
					*reinterpret_cast<uint64_t*>(obj) = *reinterpret_cast<const uint64_t*>(&src);
				} else if constexpr (sizeof(T) == 16) {
#ifdef __SSE2__
					_mm_storeu_si128(reinterpret_cast<__m128i*>(obj), _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src)));
#else
					std::memcpy(obj, &src, 16);
#endif
				} else if constexpr (sizeof(T) == 32) {
#ifdef __AVX__
					_mm256_storeu_si256(reinterpret_cast<__m256i*>(obj), _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&src)));
#else
					std::memcpy(obj, &src, 32);
#endif
				} else if constexpr (sizeof(T) == 64) {
#ifdef __AVX512F__
					_mm512_storeu_si512(reinterpret_cast<__m512i*>(obj), _mm512_loadu_si512(reinterpret_cast<const __m512i*>(&src)));
#else
					std::memcpy(obj, &src, 64);
#endif
				} else {
					std::memcpy(obj, &src, sizeof(T));
				}
				return;
			}
		}

		if constexpr (std::is_scalar_v<T> && sizeof...(Args) > 0) {
			try {
				*obj = T(std::forward<Args>(args)...);
			} catch (...) {
			}
			return;
		}

		if constexpr (!std::is_trivially_destructible_v<T> || sizeof...(Args) > 0) {
			try {
				if constexpr (!std::is_trivially_destructible_v<T>) {
					std::destroy_at(obj);
				}
				std::construct_at(obj, std::forward<Args>(args)...);
			} catch (...) {
			}
		}
	}
};

template <typename T, size_t PoolSize, bool EnableStats, typename Allocator, size_t LocalCacheSize>
thread_local typename OptimizedObjectPool<T, PoolSize, EnableStats, Allocator, LocalCacheSize>::ThreadCache
	OptimizedObjectPool<T, PoolSize, EnableStats, Allocator, LocalCacheSize>::thread_cache;

/**
 * @brief Shared pointer wrapper for object pool with automatic RAII management
 *
 * Provides std::shared_ptr interface while using object pool for allocation.
 * Objects are automatically returned to pool when shared_ptr reference count
 * reaches zero. Combines pool performance with familiar shared_ptr semantics.
 *
 * @tparam T Type of objects to pool
 * @tparam PoolSize Maximum capacity of the underlying pool
 * @tparam EnableStats Enable statistics collection
 * @tparam Allocator Allocator type for pool
 * @tparam LocalCacheSize Thread-local cache size
 */
template <typename T, size_t PoolSize = lockfree_config::DEFAULT_POOL_SIZE, bool EnableStats = false, typename Allocator = std::pmr::polymorphic_allocator<T>, size_t LocalCacheSize = lockfree_config::DEFAULT_LOCAL_CACHE_SIZE>
class SharedOptimizedObjectPool {
public:
	/**
	 * @brief Default constructor using default memory resource
	 */
	SharedOptimizedObjectPool() :
		SharedOptimizedObjectPool(Allocator { std::pmr::get_default_resource() }) { }

	/**
	 * @brief Construct with custom allocator
	 * @param allocator Custom allocator instance to use for underlying pool
	 */
	explicit SharedOptimizedObjectPool(const Allocator &allocator) :
		m_pool(allocator) { }

	/**
	 * @brief Acquire object wrapped in shared_ptr with custom deleter
	 * @tparam Args Constructor argument types
	 * @param args Arguments to forward to object constructor
	 * @return shared_ptr managing pooled object, or nullptr on failure
	 *
	 * Creates shared_ptr with custom deleter that returns object to pool
	 * when reference count reaches zero. Provides RAII semantics while
	 * maintaining pool performance benefits.
	 */
	template <typename... Args>
	[[nodiscard]] std::shared_ptr<T> acquire(Args &&... args) {
		T* raw = m_pool.acquire(std::forward<Args>(args)...);
		if (!raw) [[unlikely]] {
			return nullptr;
		}

		return std::shared_ptr<T>(raw, [this](T* ptr) noexcept {
			if (ptr) {
				m_pool.release(ptr);
			}
		});
	}

	/**
	 * @brief Pre-populate underlying pool with objects
	 * @param count Number of objects to create and add to pool
	 */
	void prewarm(size_t count) {
		m_pool.prewarm(count);
	}

	/**
	 * @brief Flush current thread's cache to global pool
	 */
	void flush_local_cache() {
		m_pool.flush_local_cache();
	}

	/**
	 * @brief Shrink pool by destroying excess objects
	 * @param max Maximum number of objects to destroy
	 * @return Number of objects actually destroyed
	 */
	[[nodiscard]] size_t shrink(size_t max = PoolSize) {
		return m_pool.shrink(max);
	}

	/**
	 * @brief Get performance statistics from underlying pool
	 * @return PoolStatistics with current performance metrics
	 */
	[[nodiscard]] auto get_stats() const {
		return m_pool.get_stats();
	}

	/**
	 * @brief Get compile-time pool capacity
	 * @return Maximum pool size
	 */
	[[nodiscard]] static constexpr size_t capacity() noexcept {
		return PoolSize;
	}

private:
	OptimizedObjectPool<T, PoolSize, EnableStats, Allocator, LocalCacheSize> m_pool;
};
