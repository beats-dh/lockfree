#pragma once

#include "base.hpp"
#include "multithreaded_benchmarks.hpp"
#include <barrier>
#include <queue>
#include <mutex>

namespace benchmark {

	/**
	 * @brief Stress testing and edge cases
	 */
	class StressBenchmarks : public BenchmarkBase {
	public:

		/**
		 * @brief High contention test with configurable parameters
		 */
		template <size_t PoolSize, size_t LocalCacheSize>
		static void benchmarkHighContention(size_t threads, size_t ops_per_thread) {
			using PoolType = OptimizedObjectPool<LargeTestObject, PoolSize, true, 
				std::pmr::polymorphic_allocator<LargeTestObject>, LocalCacheSize>;
			PoolType contention_pool;
			contention_pool.prewarm(PoolSize / 16);

			std::string name = "RawPool[P=" + std::to_string(PoolSize) + 
							  ",C=" + std::to_string(LocalCacheSize) + "] HC";
			
			auto result = MultithreadedBenchmarks::benchmarkPoolMT(name, threads, ops_per_thread, contention_pool);
			printResult(result);
		}

		/**
		 * @brief High contention analysis
		 */
		static void benchmarkHighContentionDetailed() {
			printSubsectionHeader("High Contention Analysis");

			const size_t contention_ops = 5000;
			const std::vector<size_t> contention_threads = { 4, 8, 16 };

			for (auto threads : contention_threads) {
				std::cout << "\n" << threads << " threads (cache=1, high contention):\n";
				benchmarkHighContention<256, 1>(threads, contention_ops);
				benchmarkHighContention<512, 1>(threads, contention_ops);
				benchmarkHighContention<1024, 1>(threads, contention_ops);
			}
		}

		/**
		 * @brief Memory pressure testing
		 */
		static void benchmarkMemoryPressure() {
			printSubsectionHeader("Memory Pressure Test");

			const size_t pressure_ops = 1000;

			// Test 1: make_shared pressure
			{
				auto start = Clock::now();
				std::vector<std::shared_ptr<LargeTestObject>> objects;
				objects.reserve(pressure_ops);

				for (size_t i = 0; i < pressure_ops; ++i) {
					objects.push_back(std::make_shared<LargeTestObject>());
					objects.back()->writeUInt32(static_cast<uint32_t>(i));
				}

				auto time = Duration(Clock::now() - start).count();
				std::cout << "make_shared pressure (" << pressure_ops << " objects): " 
						  << std::fixed << std::setprecision(3) << time << " ms\n";
			}

			// Test 2: SharedPool pressure
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 
					lockfree_config::DEFAULT_POOL_SIZE, true>;
				PoolType pool;
				pool.prewarm(512);

				auto start = Clock::now();
				std::vector<std::shared_ptr<LargeTestObject>> objects;
				objects.reserve(pressure_ops);

				for (size_t i = 0; i < pressure_ops; ++i) {
					objects.push_back(pool.acquire());
					if (objects.back()) {
						objects.back()->writeUInt32(static_cast<uint32_t>(i));
					}
				}

				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();
				
				std::cout << "SharedPool pressure (" << pressure_ops << " objects): " 
						  << std::fixed << std::setprecision(3) << time << " ms\n";
				std::cout << "  Pool creates: " << stats.creates << " ("
						  << std::fixed << std::setprecision(1)
						  << (stats.creates * 100.0 / stats.acquires) << "%)\n";
			}
		}

		/**
		 * @brief Thread lifecycle testing
		 */
		static void benchmarkThreadLifecycle() {
			printSubsectionHeader("Thread Lifecycle Test");

			using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, true>;
			PoolType pool;
			pool.prewarm(128);

			auto start = Clock::now();
			std::atomic<size_t> total_ops { 0 };

			for (int cycle = 0; cycle < 5; ++cycle) {
				std::vector<std::thread> threads;

				for (int t = 0; t < 4; ++t) {
					threads.emplace_back([&pool, &total_ops]() {
						for (int i = 0; i < 1000; ++i) {
							auto obj = pool.acquire();
							if (obj) {
								obj->writeByte(42);
								total_ops.fetch_add(1);
							}
						}
					});
				}

				for (auto &thread : threads) {
					thread.join();
				}
			}

			auto time = Duration(Clock::now() - start).count();
			auto stats = pool.get_stats();

			std::cout << "Thread lifecycle (" << total_ops << " total ops): " 
					  << std::fixed << std::setprecision(3) << time << " ms\n";
			std::cout << "Final objects in use: " << stats.in_use << " (should be 0)\n";
			std::cout << "Creates vs Acquires ratio: " 
					  << std::fixed << std::setprecision(1)
					  << (stats.creates * 100.0 / stats.acquires) << "%\n";
		}

		/**
		 * @brief Thread contention analysis with detailed metrics
		 */
		static void benchmarkThreadContentionAnalysis() {
			printSubsectionHeader("Thread Contention Analysis");

			const size_t contention_ops = 2000;
			const std::vector<size_t> cache_sizes = {1, 4, 8, 16, 32};
			const std::vector<size_t> thread_counts = {2, 4, 8};

			for (auto threads : thread_counts) {
				if (threads > std::thread::hardware_concurrency() * 2) continue;
				
				std::cout << "\n" << threads << " threads contention analysis:\n";
				std::cout << std::string(50, '-') << "\n";

				for (auto cache_size : cache_sizes) {
					switch (cache_size) {
						case 1: {
							using PoolType = OptimizedObjectPool<LargeTestObject, 1024, true, 
								std::pmr::polymorphic_allocator<LargeTestObject>, 1>;
							PoolType pool;
							pool.prewarm(256);
							
							auto result = MultithreadedBenchmarks::benchmarkPoolMT("Cache=" + std::to_string(cache_size), 
								threads, contention_ops, pool);
							auto stats = pool.get_stats();
							
							std::cout << "  Cache=" << cache_size << ": " 
									  << std::fixed << std::setprecision(1) << result.avg_time_ms << "ms, "
									  << "X-thread=" << (stats.cross_thread_ops * 100.0 / stats.acquires) << "%\n";
							break;
						}
						case 4: {
							using PoolType = OptimizedObjectPool<LargeTestObject, 1024, true, 
								std::pmr::polymorphic_allocator<LargeTestObject>, 4>;
							PoolType pool;
							pool.prewarm(256);
							
							auto result = MultithreadedBenchmarks::benchmarkPoolMT("Cache=" + std::to_string(cache_size), 
								threads, contention_ops, pool);
							auto stats = pool.get_stats();
							
							std::cout << "  Cache=" << cache_size << ": " 
									  << std::fixed << std::setprecision(1) << result.avg_time_ms << "ms, "
									  << "X-thread=" << (stats.cross_thread_ops * 100.0 / stats.acquires) << "%\n";
							break;
						}
						case 8: {
							using PoolType = OptimizedObjectPool<LargeTestObject, 1024, true, 
								std::pmr::polymorphic_allocator<LargeTestObject>, 8>;
							PoolType pool;
							pool.prewarm(256);
							
							auto result = MultithreadedBenchmarks::benchmarkPoolMT("Cache=" + std::to_string(cache_size), 
								threads, contention_ops, pool);
							auto stats = pool.get_stats();
							
							std::cout << "  Cache=" << cache_size << ": " 
									  << std::fixed << std::setprecision(1) << result.avg_time_ms << "ms, "
									  << "X-thread=" << (stats.cross_thread_ops * 100.0 / stats.acquires) << "%\n";
							break;
						}
						case 16: {
							using PoolType = OptimizedObjectPool<LargeTestObject, 1024, true, 
								std::pmr::polymorphic_allocator<LargeTestObject>, 16>;
							PoolType pool;
							pool.prewarm(256);
							
							auto result = MultithreadedBenchmarks::benchmarkPoolMT("Cache=" + std::to_string(cache_size), 
								threads, contention_ops, pool);
							auto stats = pool.get_stats();
							
							std::cout << "  Cache=" << cache_size << ": " 
									  << std::fixed << std::setprecision(1) << result.avg_time_ms << "ms, "
									  << "X-thread=" << (stats.cross_thread_ops * 100.0 / stats.acquires) << "%\n";
							break;
						}
						case 32: {
							using PoolType = OptimizedObjectPool<LargeTestObject, 1024, true, 
								std::pmr::polymorphic_allocator<LargeTestObject>, 32>;
							PoolType pool;
							pool.prewarm(256);
							
							auto result = MultithreadedBenchmarks::benchmarkPoolMT("Cache=" + std::to_string(cache_size), 
								threads, contention_ops, pool);
							auto stats = pool.get_stats();
							
							std::cout << "  Cache=" << cache_size << ": " 
									  << std::fixed << std::setprecision(1) << result.avg_time_ms << "ms, "
									  << "X-thread=" << (stats.cross_thread_ops * 100.0 / stats.acquires) << "%\n";
							break;
						}
					}
				}
			}
		}

		/**
		 * @brief Producer-consumer pattern test
		 */
		static void benchmarkProducerConsumer() {
			printSubsectionHeader("Producer-Consumer Pattern");

			using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, true>;
			PoolType pool;
			pool.prewarm(128);

			std::queue<std::shared_ptr<LargeTestObject>> queue;
			std::mutex queue_mutex;
			std::atomic<bool> done { false };
			std::atomic<size_t> produced { 0 }, consumed { 0 };

			auto start = Clock::now();

			std::thread producer([&]() {
				for (int i = 0; i < 5000; ++i) {
					auto obj = pool.acquire();
					if (obj) {
						obj->writeUInt32(static_cast<uint32_t>(i));
						{
							std::lock_guard<std::mutex> lock(queue_mutex);
							queue.push(obj);
						}
						produced.fetch_add(1);
					}
				}
				done = true;
			});

			std::thread consumer([&]() {
				while (!done || !queue.empty()) {
					std::shared_ptr<LargeTestObject> obj;
					{
						std::lock_guard<std::mutex> lock(queue_mutex);
						if (!queue.empty()) {
							obj = queue.front();
							queue.pop();
						}
					}
					if (obj) {
						obj->writeByte(42);
						consumed.fetch_add(1);
					}
				}
			});

			producer.join();
			consumer.join();

			auto time = Duration(Clock::now() - start).count();
			auto stats = pool.get_stats();

			std::cout << "Producer-consumer pattern: " 
					  << std::fixed << std::setprecision(3) << time << " ms\n";
			std::cout << "Produced: " << produced << ", Consumed: " << consumed << "\n";
			std::cout << "Cross-thread operations: " << stats.cross_thread_ops << " ("
					  << std::fixed << std::setprecision(1)
					  << (stats.cross_thread_ops * 100.0 / stats.acquires) << "%)\n";
		}

		/**
		 * @brief Allocation burst testing
		 */
		static void benchmarkAllocationBursts() {
			printSubsectionHeader("Allocation Burst Test");

			const size_t burst_size = 1000;
			const size_t num_bursts = 50;

			// Test 1: make_shared bursts
			{
				auto start = Clock::now();
				
				for (size_t burst = 0; burst < num_bursts; ++burst) {
					std::vector<std::shared_ptr<LargeTestObject>> objects;
					objects.reserve(burst_size);
					
					// Allocation burst
					for (size_t i = 0; i < burst_size; ++i) {
						objects.push_back(std::make_shared<LargeTestObject>());
						objects.back()->writeUInt32(static_cast<uint32_t>(i));
					}
					
					// Immediate deallocation
					objects.clear();
				}

				auto time = Duration(Clock::now() - start).count();
				std::cout << "make_shared bursts: " 
						  << std::fixed << std::setprecision(3) << time << " ms\n";
			}

			// Test 2: Pool bursts
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 2048, true>;
				PoolType pool;
				pool.prewarm(burst_size);

				auto start = Clock::now();
				
				for (size_t burst = 0; burst < num_bursts; ++burst) {
					std::vector<std::shared_ptr<LargeTestObject>> objects;
					objects.reserve(burst_size);
					
					// Allocation burst
					for (size_t i = 0; i < burst_size; ++i) {
						auto obj = pool.acquire();
						if (obj) {
							obj->writeUInt32(static_cast<uint32_t>(i));
							objects.push_back(obj);
						}
					}
					
					// Immediate deallocation
					objects.clear();
				}

				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();
				
				std::cout << "Pool bursts:        " 
						  << std::fixed << std::setprecision(3) << time << " ms";
				std::cout << " (Reuse: " << std::setprecision(1) 
						  << ((stats.acquires - stats.creates) * 100.0 / stats.acquires) << "%)\n";
			}
		}

		/**
		 * @brief Run all stress tests
		 */
		static void runStressBenchmarks() {
			printSectionHeader("STRESS & EDGE CASE TESTING", 5);
			
			benchmarkHighContentionDetailed();
			benchmarkMemoryPressure();
			benchmarkThreadLifecycle();
			benchmarkThreadContentionAnalysis();
			benchmarkProducerConsumer();
			benchmarkAllocationBursts();
		}
	};

} // namespace benchmark
