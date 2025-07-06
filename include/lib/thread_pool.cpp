/**
 * LockFree Object Pool - A high-performance, thread-safe object pool implementation
 * Copyright (©) 2025 Daniel <daniel15042015@gmail.com>
 * Repository: https://github.com/beats-dh/lockfree
 * License: https://github.com/beats-dh/lockfree/blob/main/LICENSE
 * Contributors: https://github.com/beats-dh/lockfree/graphs/contributors
 * Website: 
 */

#include "thread_pool.hpp"
#include <spdlog/spdlog.h>
#include <csignal>
#include <algorithm>

/**
 * Regardless of how many cores your computer have, we want at least
 * 4 threads because, even though they won't improve processing they
 * will make processing non-blocking in some way and that would allow
 * single core computers to process things concurrently, but not in parallel.
 */

#ifndef DEFAULT_NUMBER_OF_THREADS
	#define DEFAULT_NUMBER_OF_THREADS 4
#endif

ThreadPool &ThreadPool::getInstance() {
	static ThreadPool instance;
	return instance;
}

ThreadPool::ThreadPool(uint32_t threadCount) :
	pool { std::make_unique<BS::thread_pool<BS::tp::none>>(
		threadCount > 0 ? threadCount : std::max<uint32_t>(std::thread::hardware_concurrency(), DEFAULT_NUMBER_OF_THREADS)
	) } {
	start();
}

void ThreadPool::start() const {
	SPDLOG_INFO("Running with {} threads.", get_thread_count());
}

void ThreadPool::shutdown() {
	if (stopped) {
		return;
	}

	stopped = true;

	SPDLOG_INFO("Shutting down thread pool...");
	pool.reset();

	std::signal(SIGINT, SIG_DFL);
	std::signal(SIGTERM, SIG_DFL);

	SPDLOG_INFO("Thread pool shutdown complete.");
}
