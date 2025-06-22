#pragma once

#include "base.hpp"
#include <algorithm>

namespace benchmark {

	/**
	 * @brief Advanced analysis and comparison benchmarks
	 */
	class AnalysisBenchmarks : public BenchmarkBase {
	public:
		/**
		 * @brief Fragmentation testing
		 */
		static void benchmarkFragmentation() {
			printSubsectionHeader("Fragmentation Test");

			// Test 1: Traditional allocation with fragmentation
			{
				auto start = Clock::now();
				std::vector<LargeTestObject*> objects;

				for (int cycle = 0; cycle < 10; ++cycle) {
					for (int i = 0; i < 100; ++i) {
						objects.push_back(new LargeTestObject());
					}

					// Delete every other object to create fragmentation
					for (size_t i = 0; i < objects.size(); i += 2) {
						delete objects[i];
						objects[i] = nullptr;
					}

					objects.erase(
						std::remove(objects.begin(), objects.end(), nullptr),
						objects.end()
					);
				}

				for (auto* obj : objects) {
					delete obj;
				}

				auto time = Duration(Clock::now() - start).count();
				std::cout << "new/delete (fragmented): "
						  << std::fixed << std::setprecision(3) << time << " ms\n";
			}

			// Test 2: Pool allocation (no fragmentation)
			{
				using PoolType = OptimizedObjectPool<LargeTestObject, lockfree_config::DEFAULT_POOL_SIZE, true>;
				PoolType pool;
				pool.prewarm(128);

				auto start = Clock::now();
				std::vector<LargeTestObject*> objects;

				for (int cycle = 0; cycle < 10; ++cycle) {
					for (int i = 0; i < 100; ++i) {
						objects.push_back(pool.acquire());
					}

					// Release every other object
					for (size_t i = 0; i < objects.size(); i += 2) {
						if (objects[i]) {
							pool.release(objects[i]);
							objects[i] = nullptr;
						}
					}

					objects.erase(
						std::remove(objects.begin(), objects.end(), nullptr),
						objects.end()
					);
				}

				for (auto* obj : objects) {
					if (obj) {
						pool.release(obj);
					}
				}

				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				std::cout << "Pool (no fragmentation): "
						  << std::fixed << std::setprecision(3) << time << " ms\n";
				std::cout << "  Pool efficiency: "
						  << std::setprecision(1)
						  << ((stats.acquires - stats.creates) * 100.0 / stats.acquires) << "% reused\n";
			}
		}

		/**
		 * @brief Shared_ptr overhead analysis
		 */
		static void benchmarkSharedPtrOverhead() {
			printSubsectionHeader("Shared_ptr Overhead Analysis");

			const size_t test_ops = 50000;

			// Test 1: Raw new/delete baseline
			{
				auto start = Clock::now();
				for (size_t i = 0; i < test_ops; ++i) {
					auto* obj = new LargeTestObject();
					obj->writeByte(42);
					delete obj;
				}
				auto time = Duration(Clock::now() - start).count();
				std::cout << "Raw new/delete:      "
						  << std::fixed << std::setprecision(3) << std::setw(8) << time << " ms\n";
			}

			// Test 2: shared_ptr with new
			{
				auto start = Clock::now();
				for (size_t i = 0; i < test_ops; ++i) {
					std::shared_ptr<LargeTestObject> obj(new LargeTestObject());
					obj->writeByte(42);
				}
				auto time = Duration(Clock::now() - start).count();
				std::cout << "shared_ptr(new):     "
						  << std::fixed << std::setprecision(3) << std::setw(8) << time << " ms\n";
			}

			// Test 3: make_shared
			{
				auto start = Clock::now();
				for (size_t i = 0; i < test_ops; ++i) {
					auto obj = std::make_shared<LargeTestObject>();
					obj->writeByte(42);
				}
				auto time = Duration(Clock::now() - start).count();
				std::cout << "make_shared:         "
						  << std::fixed << std::setprecision(3) << std::setw(8) << time << " ms\n";
			}

			// Test 4: SharedPool (optimized)
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, true>;
				PoolType pool;
				pool.prewarm(64);

				auto start = Clock::now();
				for (size_t i = 0; i < test_ops; ++i) {
					auto obj = pool.acquire();
					if (obj) {
						obj->writeByte(42);
					}
				}
				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				std::cout << "SharedPool:          "
						  << std::fixed << std::setprecision(3) << std::setw(8) << time << " ms";
				std::cout << " (Cache: " << std::setprecision(1)
						  << (stats.same_thread_hits * 100.0 / stats.acquires) << "%)\n";
			}

			// Test 5: Raw pool with custom deleter
			{
				using PoolType = OptimizedObjectPool<LargeTestObject, 512, true>;
				PoolType pool;
				pool.prewarm(64);

				auto start = Clock::now();
				for (size_t i = 0; i < test_ops; ++i) {
					auto* raw = pool.acquire();
					if (raw) {
						std::shared_ptr<LargeTestObject> obj(raw, [&pool](LargeTestObject* p) {
							pool.release(p);
						});
						obj->writeByte(42);
					}
				}
				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				std::cout << "RawPool->shared_ptr: "
						  << std::fixed << std::setprecision(3) << std::setw(8) << time << " ms";
				std::cout << " (Cache: " << std::setprecision(1)
						  << (stats.same_thread_hits * 100.0 / stats.acquires) << "%)\n";
			}
		}

		/**
		 * @brief Pool configuration impact analysis
		 */
		static void benchmarkConfigurationImpact() {
			printSubsectionHeader("Pool Configuration Impact Analysis");

			const size_t config_ops = 5000;

			struct ConfigResult {
				std::string config;
				double time_ms;
				double cache_hit_rate;
				double memory_mb;
			};

			std::vector<ConfigResult> results;

			// Test different configurations
			{
				using Pool = SharedOptimizedObjectPool<LargeTestObject, 256, true, std::pmr::polymorphic_allocator<LargeTestObject>, 8>;
				Pool pool;
				pool.prewarm(32);

				auto start = Clock::now();
				for (size_t i = 0; i < config_ops; ++i) {
					auto obj = pool.acquire();
					if (obj) obj->writeByte(42);
				}
				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				results.push_back({"Pool[256,8]", time,
					stats.same_thread_hits * 100.0 / stats.acquires,
					(256 * sizeof(LargeTestObject)) / (1024.0 * 1024.0)});
			}

			{
				using Pool = SharedOptimizedObjectPool<LargeTestObject, 512, true, std::pmr::polymorphic_allocator<LargeTestObject>, 16>;
				Pool pool;
				pool.prewarm(64);

				auto start = Clock::now();
				for (size_t i = 0; i < config_ops; ++i) {
					auto obj = pool.acquire();
					if (obj) obj->writeByte(42);
				}
				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				results.push_back({"Pool[512,16]", time,
					stats.same_thread_hits * 100.0 / stats.acquires,
					(512 * sizeof(LargeTestObject)) / (1024.0 * 1024.0)});
			}

			{
				using Pool = SharedOptimizedObjectPool<LargeTestObject, 1024, true, std::pmr::polymorphic_allocator<LargeTestObject>, 32>;
				Pool pool;
				pool.prewarm(128);

				auto start = Clock::now();
				for (size_t i = 0; i < config_ops; ++i) {
					auto obj = pool.acquire();
					if (obj) obj->writeByte(42);
				}
				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				results.push_back({"Pool[1024,32]", time,
					stats.same_thread_hits * 100.0 / stats.acquires,
					(1024 * sizeof(LargeTestObject)) / (1024.0 * 1024.0)});
			}

			// Print results table
			std::cout << "Configuration comparison:\n";
			std::cout << "┌─────────────┬──────────┬───────────┬────────────┐\n";
			std::cout << "│ Config      │ Time(ms) │ Cache(%)  │ Memory(MB) │\n";
			std::cout << "├─────────────┼──────────┼───────────┼────────────┤\n";

			for (const auto& result : results) {
				std::cout << "│ " << std::left << std::setw(11) << result.config
						  << " │ " << std::right << std::setw(8) << std::fixed << std::setprecision(2) << result.time_ms
						  << " │ " << std::setw(9) << std::setprecision(1) << result.cache_hit_rate
						  << " │ " << std::setw(10) << std::setprecision(1) << result.memory_mb << " │\n";
			}
			std::cout << "└─────────────┴──────────┴───────────┴────────────┘\n";
		}

		/**
		 * @brief Copy-on-write pattern testing
		 */
		static void benchmarkCopyOnWrite() {
			printSubsectionHeader("Copy-on-Write Pattern Test");

			const size_t iterations = 10000;
			const size_t num_readers = 8;
			const size_t copy_frequency = 100; // Copy every 100 reads

			// Test 1: Traditional COW with make_shared
			{
				auto start = Clock::now();

				// Create initial shared object
				auto original = std::make_shared<LargeTestObject>();
				original->writeString("original data");
				original->writeUInt32(12345);

				std::vector<std::shared_ptr<LargeTestObject>> copies;
				copies.reserve(iterations / copy_frequency);

				// Simulate COW pattern
				for (size_t i = 0; i < iterations; ++i) {
					// Multiple readers access the same object
					for (size_t reader = 0; reader < num_readers; ++reader) {
						volatile auto checksum = original->getChecksum();
						(void)checksum; // Prevent optimization
					}

					// Occasionally make a copy (write operation)
					if (i % copy_frequency == 0) {
						// COW: create new copy for modification
						auto copy = std::make_shared<LargeTestObject>(*original);
						copy->writeString(("modified data " + std::to_string(i)).c_str());
						copy->writeUInt32(static_cast<uint32_t>(i));
						copies.push_back(copy);

						// Update original reference for next iteration
						original = copy;
					}
				}

				auto time = Duration(Clock::now() - start).count();
				std::cout << "COW with make_shared: " << std::fixed << std::setprecision(3)
						  << time << " ms (Copies: " << copies.size() << ")\n";
			}

			// Test 2: COW with pool
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 256, true>;
				PoolType pool;
				pool.prewarm(32);

				auto start = Clock::now();

				// Create initial object from pool
				auto original = pool.acquire();
				if (original) {
					original->writeString("original data");
					original->writeUInt32(12345);
				}

				std::vector<std::shared_ptr<LargeTestObject>> copies;
				copies.reserve(iterations / copy_frequency);

				// Simulate COW pattern with pool
				for (size_t i = 0; i < iterations; ++i) {
					// Multiple readers access the same object
					if (original) {
						for (size_t reader = 0; reader < num_readers; ++reader) {
							volatile auto checksum = original->getChecksum();
							(void)checksum; // Prevent optimization
						}
					}

					// Occasionally make a copy (write operation)
					if (i % copy_frequency == 0) {
						// COW: acquire new object from pool for modification
						auto copy = pool.acquire();
						if (copy && original) {
							// Copy data from original (efficient with pool objects)
							copy->writeString("original data");
							copy->writeUInt32(12345);

							// Apply modification
							copy->writeString(("modified data " + std::to_string(i)).c_str());
							copy->writeUInt32(static_cast<uint32_t>(i));

							copies.push_back(copy);

							// Update original reference for next iteration
							original = copy;
						}
					}
				}

				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				std::cout << "COW with pool:       " << std::fixed << std::setprecision(3)
						  << time << " ms (Creates: " << stats.creates
						  << ", Reuse: " << std::setprecision(1)
						  << ((stats.acquires - stats.creates) * 100.0 / stats.acquires) << "%)\n";
			}

			// Test 3: Advanced COW with reference counting optimization
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 256, true>;
				PoolType pool;
				pool.prewarm(32);

				auto start = Clock::now();

				auto original = pool.acquire();
				if (original) {
					original->writeString("original data");
					original->writeUInt32(12345);
				}

				std::vector<std::shared_ptr<LargeTestObject>> all_objects;
				all_objects.reserve(iterations);

				// Simulate advanced COW with sharing optimization
				for (size_t i = 0; i < iterations; ++i) {
					// Check if we need to copy (simulate reference count check)
					bool need_copy = (i % copy_frequency == 0);

					if (need_copy && original) {
						// Create new instance only when necessary
						auto new_obj = pool.acquire();
						if (new_obj) {
							// Efficient copy operation
							new_obj->writeString("original data");
							new_obj->writeUInt32(12345);
							new_obj->writeString(("modified " + std::to_string(i)).c_str());

							all_objects.push_back(new_obj);
							original = new_obj;
						}
					} else {
						// Just share the existing object
						if (original) {
							all_objects.push_back(original);
						}
					}
				}

				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				// Calculate sharing efficiency
				size_t unique_objects = stats.creates;
				size_t total_references = all_objects.size();
				double sharing_ratio = (total_references - unique_objects) * 100.0 / total_references;

				std::cout << "COW with sharing:    " << std::fixed << std::setprecision(3)
						  << time << " ms (Objects: " << unique_objects
						  << ", Sharing: " << std::setprecision(1) << sharing_ratio << "%)\n";
			}

			// Test 4: Multi-threaded COW simulation
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, true>;
				PoolType pool;
				pool.prewarm(64);

				const size_t num_threads = std::min(std::thread::hardware_concurrency(), 4u);
				const size_t ops_per_thread = iterations / num_threads;

				auto start = Clock::now();

				// Shared initial object
				auto shared_original = pool.acquire();
				if (shared_original) {
					shared_original->writeString("shared original");
					shared_original->writeUInt32(99999);
				}

				std::vector<std::thread> threads;
				std::atomic<size_t> total_copies{0};

				for (size_t t = 0; t < num_threads; ++t) {
					threads.emplace_back([&, t]() {
						auto& local_obj = shared_original; // Start with shared object

						for (size_t i = 0; i < ops_per_thread; ++i) {
							// Read operations (no copy needed)
							if (local_obj) {
								volatile auto checksum = local_obj->getChecksum();
								(void)checksum;
							}

							// Occasional write (needs copy)
							if (i % (copy_frequency * 2) == 0) {
								auto new_obj = pool.acquire();
								if (new_obj && local_obj) {
									// Copy data
									new_obj->writeString("shared original");
									new_obj->writeUInt32(99999);

									// Apply thread-specific modification
									new_obj->writeString(("thread " + std::to_string(t) + " mod " + std::to_string(i)).c_str());

									local_obj = new_obj;
									total_copies.fetch_add(1);
								}
							}
						}
					});
				}

				for (auto& thread : threads) {
					thread.join();
				}

				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				std::cout << "COW multi-threaded:  " << std::fixed << std::setprecision(3)
						  << time << " ms (Threads: " << num_threads
						  << ", Copies: " << total_copies
						  << ", Cross-thread: " << std::setprecision(1)
						  << (stats.cross_thread_ops * 100.0 / stats.acquires) << "%)\n";
			}
		}

		/**
		 * @brief Memory usage analysis
		 */
		static void analyzeMemoryUsage() {
			printSubsectionHeader("Memory Usage Analysis");

			constexpr size_t obj_size = sizeof(LargeTestObject);
			// constexpr size_t kb = 1024;
			// constexpr size_t mb = kb * kb;

			std::cout << "Object size:       " << obj_size << " bytes (~"
					  << std::fixed << std::setprecision(1) << (obj_size / 1024.0) << " KB)\n";

			const std::vector<size_t> pool_sizes = { 256, 512, 1024, 2048 };
			std::cout << "\nPool memory usage:\n";
			for (const auto size : pool_sizes) {
				std::cout << "  Pool size " << std::setw(4) << size << ":    ~"
						  << std::setprecision(1) << ((obj_size * size) / (1024.0 * 1024.0)) << " MB\n";
			}

			const std::vector<size_t> cache_sizes = { 8, 16, 32, 64 };
			std::cout << "\nThread cache memory usage:\n";
			for (auto size : cache_sizes) {
				std::cout << "  Cache size " << std::setw(2) << size << ":     ~"
						  << std::setprecision(1) << ((obj_size * size) / 1024.0) << " KB per thread\n";
			}
		}

		/**
		 * @brief Performance regression testing
		 */
		static void benchmarkPerformanceRegression() {
			printSubsectionHeader("Performance Regression Test");

			const size_t regression_ops = 25000;

			std::cout << "Testing performance consistency (5 runs):\n";

			using PoolType = SharedOptimizedObjectPool<LargeTestObject, 1024, true>;
			PoolType pool;
			pool.prewarm(256);

			std::vector<double> run_times;
			std::vector<double> cache_hit_rates;

			for (int run = 1; run <= 5; ++run) {
				auto start = Clock::now();

				for (size_t i = 0; i < regression_ops; ++i) {
					auto obj = pool.acquire();
					if (obj) {
						obj->writeByte(static_cast<uint8_t>(i % 256));
					}
				}

				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();
				auto hit_rate = stats.same_thread_hits * 100.0 / stats.acquires;

				run_times.push_back(time);
				cache_hit_rates.push_back(hit_rate);

				std::cout << "  Run " << run << ": "
						  << std::fixed << std::setprecision(3) << time << "ms, "
						  << "Cache: " << std::setprecision(1) << hit_rate << "%\n";
			}

			// Calculate coefficient of variation
			double avg_time = std::accumulate(run_times.begin(), run_times.end(), 0.0) / run_times.size();
			double variance = 0;
			for (double t : run_times) {
				variance += (t - avg_time) * (t - avg_time);
			}
			variance /= run_times.size();
			double cv = std::sqrt(variance) / avg_time;

			std::cout << "Performance variance (CV): "
					  << std::fixed << std::setprecision(3) << (cv * 100) << "%\n";
		}

		/**
		 * @brief Short-lived vs long-lived object patterns
		 */
		static void benchmarkObjectLifetimePatterns() {
			printSubsectionHeader("Object Lifetime Patterns");

			// Pattern 1: Short-lived objects
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, true>;
				PoolType pool;
				pool.prewarm(64);

				auto start = Clock::now();
				for (int i = 0; i < 10000; ++i) {
					auto obj = pool.acquire();
					if (obj) {
						obj->writeString("network data");
						obj->writeUInt32(static_cast<uint32_t>(i));
					}
				}

				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				std::cout << "Short-lived pattern (10k objects): "
						  << std::fixed << std::setprecision(3) << time << " ms\n";
				std::cout << "  Cache hit rate: "
						  << std::setprecision(1) << (stats.same_thread_hits * 100.0 / stats.acquires) << "%\n";
			}

			// Pattern 2: Long-lived cache pattern
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, true>;
				PoolType pool;
				pool.prewarm(64);

				auto start = Clock::now();
				std::vector<std::shared_ptr<LargeTestObject>> cache;
				cache.reserve(100);

				// Create long-lived objects
				for (int i = 0; i < 100; ++i) {
					auto obj = pool.acquire();
					if (obj) {
						obj->writeString("cached data");
						cache.push_back(obj);
					}
				}

				// Simulate usage over time
				for (int cycle = 0; cycle < 1000; ++cycle) {
					for (auto &obj : cache) {
						obj->writeByte(static_cast<uint8_t>(cycle % 256));
					}
				}

				cache.clear();

				auto time = Duration(Clock::now() - start).count();
				auto stats = pool.get_stats();

				std::cout << "Long-lived cache pattern: "
						  << std::fixed << std::setprecision(3) << time << " ms\n";
				std::cout << "  Final pool efficiency: "
						  << std::setprecision(1)
						  << ((stats.acquires - stats.creates) * 100.0 / stats.acquires) << "% reused\n";
			}
		}

		/**
		 * @brief Run all analysis benchmarks
		 */
		static void runAnalysisBenchmarks() {
			printSectionHeader("ADVANCED PERFORMANCE ANALYSIS", 6);

			benchmarkFragmentation();
			benchmarkSharedPtrOverhead();
			benchmarkCopyOnWrite();
			benchmarkObjectLifetimePatterns();
			benchmarkPerformanceRegression();
			analyzeMemoryUsage();
		}
	};

} // namespace benchmark
