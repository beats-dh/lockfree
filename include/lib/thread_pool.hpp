/**
 * LockFree Object Pool - A high-performance, thread-safe object pool implementation
 * Copyright (©) 2025 Daniel <daniel15042015@gmail.com>
 * Repository: https://github.com/beats-dh/lockfree
 * License: https://github.com/beats-dh/lockfree/blob/main/LICENSE
 * Contributors: https://github.com/beats-dh/lockfree/graphs/contributors
 * Website: 
 */

#pragma once

#include "BS_thread_pool.hpp"

class ThreadPool {
public:
	explicit ThreadPool(const uint32_t threadCount = std::thread::hardware_concurrency());

	// Ensures that we don't accidentally copy it
	ThreadPool(const ThreadPool &) = delete;
	ThreadPool &operator=(const ThreadPool &) = delete;

	static ThreadPool &getInstance();

	template <typename F>
	void detach_task(F &&f) {
		pool->detach_task(std::forward<F>(f));
	}

	template <typename F>
	auto submit_loop(std::size_t first, std::size_t last, F &&f) {
		return pool->submit_loop(first, last, std::forward<F>(f));
	}

	auto get_thread_count() const noexcept {
		return pool->get_thread_count();
	}

	void start() const;
	void shutdown();

	static int16_t getThreadId() {
		static std::atomic_int16_t counter{0};
		thread_local static int16_t id = counter.fetch_add(1, std::memory_order_relaxed);
		return id;
	}

	bool isStopped() const {
		return stopped;
	}

private:
	std::mutex mutex;
	std::condition_variable condition;

	std::atomic<bool> stopped { false };

	std::unique_ptr<BS::thread_pool<BS::tp::none>> pool;
};

