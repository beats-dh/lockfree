#pragma once

#include "base.hpp"

namespace benchmark {

	/**
	 * @brief Object pool specific benchmarks
	 */
	class PoolBenchmarks : public BenchmarkBase {
	public:

		/**
		 * @brief Enhanced vector pool shared benchmark
		 */
		static BenchmarkResult benchmarkVectorPoolShared(size_t ops) {
			using PoolType = SharedOptimizedObjectPool<LargeTestObject, 
				lockfree_config::DEFAULT_POOL_SIZE, true>; // Stats enabled
			PoolType pool;
			pool.prewarm(std::min(static_cast<size_t>(128), ops));

			std::vector<double> times;
			times.reserve(10);

			for (int run = 0; run < 10; ++run) {
				auto start = Clock::now();
				
				std::vector<std::shared_ptr<LargeTestObject>> objects;
				objects.reserve(ops);

				for (size_t i = 0; i < ops; ++i) {
					objects.push_back(pool.acquire());
					if (objects.back()) {
						objects.back()->writeString("test data");
						objects.back()->writeUInt32(static_cast<uint32_t>(i));
					}
				}

				objects.clear();
				times.push_back(Duration(Clock::now() - start).count());
			}

			auto result = calculateStats("vector<SharedPool> (ST)", times, ops, g_st_baseline_avg_ms);
			addPoolStats(result, pool);
			return result;
		}

		/**
		 * @brief Generic pool benchmark for single-threaded
		 */
		template <typename PoolType>
		static BenchmarkResult benchmarkPoolST(const std::string &pool_name, size_t ops, PoolType &pool) {
			std::vector<double> times;
			times.reserve(10);
			
			for (int run = 0; run < 10; ++run) {
				auto start = Clock::now();
				
				if constexpr (std::is_pointer_v<decltype(pool.acquire())>) {
					// Raw pointer pool
					for (size_t i = 0; i < ops; ++i) {
						auto* obj = pool.acquire();
						if (!obj) [[unlikely]] {
							continue;
						}
						obj->writeString("test data");
						obj->writeUInt32(static_cast<uint32_t>(i));
						pool.release(obj);
					}
				} else {
					// Shared pointer pool
					for (size_t i = 0; i < ops; ++i) {
						auto obj_ptr = pool.acquire();
						if (!obj_ptr) [[unlikely]] {
							continue;
						}
						obj_ptr->writeString("test data");
						obj_ptr->writeUInt32(static_cast<uint32_t>(i));
					}
				}
				times.push_back(Duration(Clock::now() - start).count());
			}
			
			auto result = calculateStats(pool_name + " (ST)", times, ops, g_st_baseline_avg_ms);
			addPoolStats(result, pool);
			return result;
		}

		/**
		 * @brief Test different SharedPool configurations
		 */
		static void benchmarkSharedPoolConfigurations(size_t ops) {
			printSubsectionHeader("SharedPool Configuration Comparison");

			// Pool 256 with stats
			{
				using Pool256 = SharedOptimizedObjectPool<LargeTestObject, 256, true>;
				Pool256 pool256;
				pool256.prewarm(64);
				printResult(benchmarkPoolST("SharedPool[P=256,Stats=ON]", ops, pool256));
			}

			// Pool 512 optimized
			{
				using Pool512 = SharedOptimizedObjectPool<LargeTestObject, 512, false>;
				Pool512 pool512;
				pool512.prewarm(128);
				printResult(benchmarkPoolST("SharedPool[P=512,Optimized]", ops, pool512));
			}

			// Pool 1024 large
			{
				using Pool1024 = SharedOptimizedObjectPool<LargeTestObject, 1024, false>;
				Pool1024 pool1024;
				pool1024.prewarm(256);
				printResult(benchmarkPoolST("SharedPool[P=1024,Large]", ops, pool1024));
			}
		}

		/**
		 * @brief Test different cache sizes
		 */
		static void benchmarkCacheSizes(size_t ops) {
			printSubsectionHeader("Thread Cache Size Analysis");
			
			const std::vector<size_t> cache_sizes = {4, 8, 16, 32, 64};
			for (auto cache_size : cache_sizes) {
				switch (cache_size) {
					case 4: {
						using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, false, 
							std::pmr::polymorphic_allocator<LargeTestObject>, 4>;
						PoolType pool;
						pool.prewarm(64);
						printResult(benchmarkPoolST("SharedPool[Cache=4]", ops, pool));
						break;
					}
					case 8: {
						using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, false, 
							std::pmr::polymorphic_allocator<LargeTestObject>, 8>;
						PoolType pool;
						pool.prewarm(64);
						printResult(benchmarkPoolST("SharedPool[Cache=8]", ops, pool));
						break;
					}
					case 16: {
						using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, false, 
							std::pmr::polymorphic_allocator<LargeTestObject>, 16>;
						PoolType pool;
						pool.prewarm(64);
						printResult(benchmarkPoolST("SharedPool[Cache=16]", ops, pool));
						break;
					}
					case 32: {
						using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, false, 
							std::pmr::polymorphic_allocator<LargeTestObject>, 32>;
						PoolType pool;
						pool.prewarm(64);
						printResult(benchmarkPoolST("SharedPool[Cache=32]", ops, pool));
						break;
					}
					case 64: {
						using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, false, 
							std::pmr::polymorphic_allocator<LargeTestObject>, 64>;
						PoolType pool;
						pool.prewarm(64);
						printResult(benchmarkPoolST("SharedPool[Cache=64]", ops, pool));
						break;
					}
				}
			}
		}

		/**
		 * @brief Run all single-threaded pool benchmarks
		 */
		static void runSingleThreadedPoolBenchmarks(size_t ops = 1000000) {  // consistent with main_test_lockfree.cpp default
			printSectionHeader("POOL vs STANDARD SHARED_PTR ANALYSIS", 2);
			std::cout << "Operations: " << ops << "\n\n";

			// Vector comparison
			const size_t vector_ops = ops / 20;
			printResult(benchmarkVectorPoolShared(vector_ops));

			// Configuration tests
			benchmarkSharedPoolConfigurations(ops);

			printSectionHeader("THREAD CACHE SIZE OPTIMIZATION", 3);
			benchmarkCacheSizes(ops);
		}
	};

} // namespace benchmark