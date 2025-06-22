#pragma once

#include "base.hpp"
#include <barrier>

namespace benchmark {

	/**
	 * @brief Multi-threaded benchmarks
	 */
	class MultithreadedBenchmarks : public BenchmarkBase {
	public:

		/**
		 * @brief Multi-threaded new/delete benchmark
		 */
		static BenchmarkResult benchmarkMultiThreadedNew(size_t threads, size_t ops_per_thread) {
			std::vector<double> times;
			times.reserve(5);
			
			for (int run = 0; run < 5; ++run) {
				std::barrier sync_point(static_cast<std::ptrdiff_t>(threads));
				std::vector<std::thread> workers;
				
				auto start = Clock::now();
				for (size_t t = 0; t < threads; ++t) {
					workers.emplace_back([&sync_point, ops_per_thread, t]() {
						sync_point.arrive_and_wait();
						for (size_t i = 0; i < ops_per_thread; ++i) {
							auto* obj = new LargeTestObject();
							obj->writeString("thread data");
							obj->writeUInt32(static_cast<uint32_t>(t * 1000 + i));
							delete obj;
						}
					});
				}
				
				for (auto &w : workers) {
					w.join();
				}
				times.push_back(Duration(Clock::now() - start).count());
			}
			
			BenchmarkResult result = calculateStats("new/delete (MT Baseline)", times, threads * ops_per_thread);
			g_mt_baseline_avg_ms[threads] = result.avg_time_ms;
			return result;
		}

		/**
		 * @brief Multi-threaded make_shared benchmark
		 */
		static BenchmarkResult benchmarkMultiThreadedMakeShared(size_t threads, size_t ops_per_thread) {
			std::vector<double> times;
			times.reserve(5);

			for (int run = 0; run < 5; ++run) {
				std::barrier sync_point(static_cast<std::ptrdiff_t>(threads));
				std::vector<std::thread> workers;

				auto start = Clock::now();
				for (size_t t = 0; t < threads; ++t) {
					workers.emplace_back([&sync_point, ops_per_thread, t]() {
						sync_point.arrive_and_wait();
						for (size_t i = 0; i < ops_per_thread; ++i) {
							auto obj = std::make_shared<LargeTestObject>();
							obj->writeString("thread data");
							obj->writeUInt32(static_cast<uint32_t>(t * 1000 + i));
						}
					});
				}

				for (auto &w : workers) {
					w.join();
				}
				times.push_back(Duration(Clock::now() - start).count());
			}

			return calculateStats("std::make_shared (MT)", times, threads * ops_per_thread, g_mt_baseline_avg_ms[threads]);
		}

		/**
		 * @brief Generic pool benchmark for multi-threaded
		 */
		template <typename PoolType>
		static BenchmarkResult benchmarkPoolMT(const std::string &pool_name, size_t threads, size_t ops_per_thread, PoolType &pool) {
			std::vector<double> times;
			times.reserve(5);

			for (int run = 0; run < 5; ++run) {
				std::barrier sync_point(static_cast<std::ptrdiff_t>(threads));
				std::vector<std::thread> workers;

				auto start = Clock::now();
				for (size_t t = 0; t < threads; ++t) {
					workers.emplace_back([&sync_point, &pool, ops_per_thread, t]() {
						sync_point.arrive_and_wait();

						if constexpr (std::is_pointer_v<decltype(pool.acquire())>) {
							// Raw pointer pool
							for (size_t i = 0; i < ops_per_thread; ++i) {
								auto* obj = pool.acquire();
								if (!obj) [[unlikely]] {
									continue;
								}
								obj->writeString("thread data");
								obj->writeUInt32(static_cast<uint32_t>(t * 1000 + i));
								pool.release(obj);
							}
						} else {
							// Shared pointer pool
							for (size_t i = 0; i < ops_per_thread; ++i) {
								auto obj_ptr = pool.acquire();
								if (!obj_ptr) [[unlikely]] {
									continue;
								}
								obj_ptr->writeString("thread data");
								obj_ptr->writeUInt32(static_cast<uint32_t>(t * 1000 + i));
							}
						}
					});
				}

				for (auto &w : workers) {
					w.join();
				}
				times.push_back(Duration(Clock::now() - start).count());
			}

			auto result = calculateStats(pool_name + " (MT)", times, threads * ops_per_thread, g_mt_baseline_avg_ms[threads]);
			addPoolStats(result, pool);
			return result;
		}

		/**
		 * @brief Test multi-threaded scaling for a specific thread count
		 */
		static void benchmarkThreadCount(size_t threads, size_t ops_per_thread) {
			std::cout << "\n" << std::string(90, '─') << "\n";
			std::cout << "🧵 " << threads << " Thread" << (threads > 1 ? "s" : "")
					  << " (" << ops_per_thread << " ops/thread, "
					  << (threads * ops_per_thread) << " total ops):\n";
			std::cout << std::string(90, '─') << "\n";

			// Reset baseline for this thread count
			g_mt_baseline_avg_ms.clear();
			printResult(benchmarkMultiThreadedNew(threads, ops_per_thread));
			printResult(benchmarkMultiThreadedMakeShared(threads, ops_per_thread));

			// Pool comparisons with detailed metrics
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 512, true>;
				PoolType pool;
				pool.prewarm(128);
				printResult(benchmarkPoolMT("SharedPool[P=512,Stats]", threads, ops_per_thread, pool));
			}
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 1024, false>;
				PoolType pool;
				pool.prewarm(256);
				printResult(benchmarkPoolMT("SharedPool[P=1024,Fast]", threads, ops_per_thread, pool));
			}
			{
				using PoolType = SharedOptimizedObjectPool<LargeTestObject, 2048, false>;
				PoolType pool;
				pool.prewarm(512);
				printResult(benchmarkPoolMT("SharedPool[P=2048,XLarge]", threads, ops_per_thread, pool));
			}
		}

		/**
		 * @brief Run complete multi-threaded scaling analysis
		 */
		static void runMultiThreadedScalingAnalysis(size_t base_ops = 5000) {
			printSectionHeader("MULTI-THREADED SCALING ANALYSIS", 4);

			auto thread_counts = generateThreadCounts();

			for (size_t threads : thread_counts) {
				const size_t ops_per_thread = std::max(base_ops / threads, size_t(1000));
				benchmarkThreadCount(threads, ops_per_thread);
			}
		}
	};

} // namespace benchmark