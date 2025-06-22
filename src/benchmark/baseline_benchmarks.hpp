#pragma once

#include "base.hpp"
#include <memory_resource>

namespace benchmark {

	// Global baseline variables definition
	double g_st_baseline_avg_ms = 0.0;
	std::map<size_t, double> g_mt_baseline_avg_ms;

	/**
	 * @brief Baseline allocation method benchmarks
	 */
	class BaselineBenchmarks : public BenchmarkBase {
	public:
		
		/**
		 * @brief Benchmark malloc/free allocation
		 */
		static BenchmarkResult benchmarkMalloc(size_t ops) {
			std::vector<double> times;
			times.reserve(10);
			
			for (int run = 0; run < 10; ++run) {
				auto start = Clock::now();
				for (size_t i = 0; i < ops; ++i) {
					void* mem = std::malloc(sizeof(LargeTestObject));
					if (!mem) [[unlikely]] {
						continue;
					}
					auto* obj = new (mem) LargeTestObject();
					obj->writeString("test data");
					obj->writeUInt32(static_cast<uint32_t>(i));
					obj->~LargeTestObject();
					std::free(mem);
				}
				times.push_back(Duration(Clock::now() - start).count());
			}
			return calculateStats("malloc/free (ST)", times, ops, g_st_baseline_avg_ms);
		}

		/**
		 * @brief Benchmark new/delete allocation (baseline)
		 */
		static BenchmarkResult benchmarkNew(size_t ops) {
			std::vector<double> times;
			times.reserve(10);
			
			for (int run = 0; run < 10; ++run) {
				auto start = Clock::now();
				for (size_t i = 0; i < ops; ++i) {
					auto* obj = new LargeTestObject();
					obj->writeString("test data");
					obj->writeUInt32(static_cast<uint32_t>(i));
					delete obj;
				}
				times.push_back(Duration(Clock::now() - start).count());
			}
			
			BenchmarkResult result = calculateStats("new/delete (ST Baseline)", times, ops);
			g_st_baseline_avg_ms = result.avg_time_ms;
			return result;
		}

		/**
		 * @brief Benchmark PMR allocator
		 */
		static BenchmarkResult benchmarkPmr(size_t ops) {
			std::pmr::synchronized_pool_resource pool_resource;
			std::pmr::polymorphic_allocator<LargeTestObject> alloc(&pool_resource);
			std::vector<double> times;
			times.reserve(10);
			
			for (int run = 0; run < 10; ++run) {
				auto start = Clock::now();
				for (size_t i = 0; i < ops; ++i) {
					auto* obj = alloc.allocate(1);
					std::construct_at(obj);
					obj->writeString("test data");
					obj->writeUInt32(static_cast<uint32_t>(i));
					std::destroy_at(obj);
					alloc.deallocate(obj, 1);
				}
				times.push_back(Duration(Clock::now() - start).count());
			}
			return calculateStats("pmr::sync_pool_res (ST)", times, ops, g_st_baseline_avg_ms);
		}

		/**
		 * @brief Benchmark make_shared
		 */
		static BenchmarkResult benchmarkMakeShared(size_t ops) {
			std::vector<double> times;
			times.reserve(10);

			for (int run = 0; run < 10; ++run) {
				auto start = Clock::now();
				for (size_t i = 0; i < ops; ++i) {
					auto obj = std::make_shared<LargeTestObject>();
					obj->writeString("test data");
					obj->writeUInt32(static_cast<uint32_t>(i));
				}
				times.push_back(Duration(Clock::now() - start).count());
			}

			return calculateStats("std::make_shared (ST)", times, ops, g_st_baseline_avg_ms);
		}

		/**
		 * @brief Benchmark allocate_shared with PMR
		 */
		static BenchmarkResult benchmarkAllocateShared(size_t ops) {
			std::pmr::synchronized_pool_resource pool_resource;
			std::pmr::polymorphic_allocator<LargeTestObject> alloc(&pool_resource);

			std::vector<double> times;
			times.reserve(10);

			for (int run = 0; run < 10; ++run) {
				auto start = Clock::now();
				for (size_t i = 0; i < ops; ++i) {
					auto obj = std::allocate_shared<LargeTestObject>(alloc);
					obj->writeString("test data");
					obj->writeUInt32(static_cast<uint32_t>(i));
				}
				times.push_back(Duration(Clock::now() - start).count());
			}

			return calculateStats("std::allocate_shared+pmr (ST)", times, ops, g_st_baseline_avg_ms);
		}

		/**
		 * @brief Benchmark vector of shared_ptr
		 */
		static BenchmarkResult benchmarkVectorSharedPtr(size_t ops) {
			std::vector<double> times;
			times.reserve(10);

			for (int run = 0; run < 10; ++run) {
				auto start = Clock::now();
				
				std::vector<std::shared_ptr<LargeTestObject>> objects;
				objects.reserve(ops);

				for (size_t i = 0; i < ops; ++i) {
					objects.push_back(std::make_shared<LargeTestObject>());
					objects.back()->writeString("test data");
					objects.back()->writeUInt32(static_cast<uint32_t>(i));
				}

				objects.clear();
				times.push_back(Duration(Clock::now() - start).count());
			}

			return calculateStats("vector<make_shared> (ST)", times, ops, g_st_baseline_avg_ms);
		}

		/**
		 * @brief Run all baseline benchmarks
		 */
		static void runBaselineBenchmarks(size_t ops = 1000000) {  // consistent with main_test_lockfree.cpp default
			printSectionHeader("BASELINE ALLOCATION METHODS", 1);
			std::cout << "Operations: " << ops << "\n\n";
			
			printResult(benchmarkNew(ops));
			printResult(benchmarkMalloc(ops));
			printResult(benchmarkPmr(ops));
			printResult(benchmarkMakeShared(ops));
			printResult(benchmarkAllocateShared(ops));
			printResult(benchmarkVectorSharedPtr(ops / 10));
		}
	};

} // namespace benchmark