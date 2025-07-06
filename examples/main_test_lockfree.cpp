/**
 * LockFree Object Pool - A high-performance, thread-safe object pool implementation
 * Copyright (©) 2025 Daniel <daniel15042015@gmail.com>
 * Repository: https://github.com/beats-dh/lockfree
 * License: https://github.com/beats-dh/lockfree/blob/main/LICENSE
 * Contributors: https://github.com/beats-dh/lockfree/graphs/contributors
 * Website: 
 */

#include "../src/benchmark/main_benchmark.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>


#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#endif

#ifdef _WIN32
/**
 * @brief Setup UTF-8 console for Windows
 */
static void setupUTF8Console() {
    // Configurar console para UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // Habilitar processamento de sequências ANSI/VT100
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
}
#endif

/**
 * @brief Print usage information
 */
static void printUsage(const char* program_name) {
	std::cout << "Object Pool Benchmark Suite\n";
	std::cout << "Usage: " << program_name << " [options]\n\n";
	std::cout << "Options:\n";
	std::cout << "  --help, -h          Show this help message\n";
	std::cout << "  --complete          Run complete benchmark suite (default)\n";
	std::cout << "  --light             Run lightweight benchmark (for CI/CD)\n";
	std::cout << "  --integration       Run only integration test\n";
	std::cout << "  --baseline          Run only baseline benchmarks\n";
	std::cout << "  --pool              Run only pool benchmarks\n";
	std::cout << "  --multithread       Run only multi-threaded benchmarks\n";
	std::cout << "  --stress            Run only stress tests\n";
	std::cout << "  --analysis          Run only advanced analysis\n";
	std::cout << "  --threadid          Run only ThreadId optimization tests\n";
	std::cout << "  --ops <number>      Set number of operations (default: auto)\n";
	std::cout << "  --threads <number>  Set max threads for testing (default: auto)\n";
	std::cout << "  --warmup <number>   Set warmup operations (default: 20000)\n";
	std::cout << "\nExamples:\n";
	std::cout << "  " << program_name << "                    # Run complete suite\n";
	std::cout << "  " << program_name << " --light            # Quick test\n";
	std::cout << "  " << program_name << " --stress --ops 5000  # Stress test with 5K ops\n";
	std::cout << "  " << program_name << " --multithread --threads 8  # MT test up to 8 threads\n";
	std::cout << "  " << program_name << " --threadid         # Test ThreadId optimization\n";
}

/**
 * @brief Parse command line arguments
 */
struct BenchmarkConfig {
	enum Mode {
		COMPLETE,
		LIGHT,
		INTEGRATION,
		BASELINE,
		POOL,
		MULTITHREAD,
		STRESS,
		ANALYSIS,
		THREADID
	} mode
		= COMPLETE;

	// Centralized benchmark constants
	static constexpr size_t DEFAULT_SINGLE_THREAD_OPS = 100000;  // default ops for COMPLETE mode
	static constexpr size_t DEFAULT_MULTI_THREAD_BASE_OPS = 50000;
	static constexpr size_t DEFAULT_WARMUP_OPS = 10000;
	static constexpr size_t DEFAULT_LIGHT_OPS = 1000;
	static constexpr size_t DEFAULT_STRESS_OPS = 5000;

	size_t ops = 0; // 0 = auto
	size_t max_threads = 0; // 0 = auto
	size_t warmup_ops = DEFAULT_WARMUP_OPS;
	bool show_help = false;
};

static BenchmarkConfig parseArgs(int argc, char* argv[]) {
	BenchmarkConfig config;

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];

		if (arg == "--help" || arg == "-h") {
			config.show_help = true;
		} else if (arg == "--complete") {
			config.mode = BenchmarkConfig::COMPLETE;
		} else if (arg == "--light") {
			config.mode = BenchmarkConfig::LIGHT;
		} else if (arg == "--integration") {
			config.mode = BenchmarkConfig::INTEGRATION;
		} else if (arg == "--baseline") {
			config.mode = BenchmarkConfig::BASELINE;
		} else if (arg == "--pool") {
			config.mode = BenchmarkConfig::POOL;
		} else if (arg == "--multithread") {
			config.mode = BenchmarkConfig::MULTITHREAD;
		} else if (arg == "--stress") {
			config.mode = BenchmarkConfig::STRESS;
		} else if (arg == "--analysis") {
			config.mode = BenchmarkConfig::ANALYSIS;
		} else if (arg == "--threadid") {
			config.mode = BenchmarkConfig::THREADID;
		} else if (arg == "--ops" && i + 1 < argc) {
			config.ops = std::stoull(argv[++i]);
		} else if (arg == "--threads" && i + 1 < argc) {
			config.max_threads = std::stoull(argv[++i]);
		} else if (arg == "--warmup" && i + 1 < argc) {
			config.warmup_ops = std::stoull(argv[++i]);
		} else {
			std::cerr << "Unknown option: " << arg << "\n";
			std::cerr << "Use --help for usage information\n";
			std::exit(1);
		}
	}

	return config;
}

/**
 * @brief Run specific benchmark modules based on configuration
 */
static void runBenchmarkModule(const BenchmarkConfig &config) {
	using namespace benchmark;

	// Determine default operations if not specified
	size_t ops = config.ops;
	if (ops == 0) {
		switch (config.mode) {
			case BenchmarkConfig::LIGHT:
				ops = BenchmarkConfig::DEFAULT_LIGHT_OPS;
				break;
			case BenchmarkConfig::STRESS:
				ops = BenchmarkConfig::DEFAULT_STRESS_OPS;
				break;
			default:
				ops = BenchmarkConfig::DEFAULT_SINGLE_THREAD_OPS;
				break;
		}
	}

	// System validation and warmup (except for integration-only)
	if (config.mode != BenchmarkConfig::INTEGRATION) {
		BenchmarkBase::validateObjectSize();
		BenchmarkBase::printSystemInfo();
		BenchmarkBase::warmup(config.warmup_ops);
	}

	switch (config.mode) {
		case BenchmarkConfig::COMPLETE:
			MainBenchmark::runCompleteBenchmarkSuite(
				BenchmarkConfig::DEFAULT_SINGLE_THREAD_OPS,
				BenchmarkConfig::DEFAULT_MULTI_THREAD_BASE_OPS,
				config.warmup_ops
			);
			break;
		case BenchmarkConfig::THREADID:
			// ThreadId optimization tests are handled in the earlier switch
			break;

		case BenchmarkConfig::LIGHT:
			MainBenchmark::runLightweightBenchmark(
				BenchmarkConfig::DEFAULT_LIGHT_OPS,
				config.warmup_ops
			);
			break;

		case BenchmarkConfig::INTEGRATION:
			MainBenchmark::runQuickIntegrationTest();
			break;

		case BenchmarkConfig::BASELINE:
			BaselineBenchmarks::runBaselineBenchmarks(ops);
			break;

		case BenchmarkConfig::POOL:
			PoolBenchmarks::runSingleThreadedPoolBenchmarks(ops);
			break;

		case BenchmarkConfig::MULTITHREAD: {
			auto thread_counts = BenchmarkBase::generateThreadCounts(config.max_threads);
			for (const size_t threads : thread_counts) {
				const size_t ops_per_thread = std::max(ops / threads, static_cast<size_t>(1000));
				MultithreadedBenchmarks::benchmarkThreadCount(threads, ops_per_thread);
			}
			break;
		}

		case BenchmarkConfig::STRESS:
			StressBenchmarks::runStressBenchmarks();
			break;

		case BenchmarkConfig::ANALYSIS:
			AnalysisBenchmarks::runAnalysisBenchmarks();
			break;
	}
}

/**
 * @brief Print system environment information
 */
static void printEnvironmentInfo() {
	std::cout << "\n📊 ENVIRONMENT INFORMATION:\n";
	std::cout << std::string(50, 0x2D) << "\n";

// Compiler information
#ifdef __GNUC__
	std::cout << "Compiler: GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << "\n";
#elif defined(__clang__)
	std::cout << "Compiler: Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__ << "\n";
#elif defined(_MSC_VER)
	std::cout << "Compiler: MSVC " << _MSC_VER << "\n";
#else
	std::cout << "Compiler: Unknown\n";
#endif

	// C++ standard
	std::cout << "C++ Standard: ";
#if __cplusplus >= 202002L
	std::cout << "C++20 or later\n";
#elif __cplusplus >= 201703L
	std::cout << "C++17\n";
#elif __cplusplus >= 201402L
	std::cout << "C++14\n";
#elif __cplusplus >= 201103L
	std::cout << "C++11\n";
#else
	std::cout << "Pre-C++11\n";
#endif

// Build configuration
#ifdef NDEBUG
	std::cout << "Build: Release (Optimized)\n";
#else
	std::cout << "Build: Debug\n";
#endif

	// Architecture
	std::cout << "Architecture: ";
#if defined(__x86_64__) || defined(_M_X64)
	std::cout << "x86_64\n";
#elif defined(__i386__) || defined(_M_IX86)
	std::cout << "x86\n";
#elif defined(__aarch64__) || defined(_M_ARM64)
	std::cout << "ARM64\n";
#elif defined(__arm__) || defined(_M_ARM)
	std::cout << "ARM\n";
#else
	std::cout << "Unknown\n";
#endif

	std::cout << std::string(50, 0x2D) << "\n";
}

/**
 * @brief Signal handler for graceful shutdown
 */
#include <csignal>
std::atomic<bool> g_shutdown_requested { false };

static void signalHandler(int signal) {
	std::cout << "\n\n⚠️  Signal " << signal << " received. Attempting graceful shutdown...\n";
	g_shutdown_requested = true;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
	try {
#ifdef _WIN32
		// Setup UTF-8 console for proper emoji/Unicode display
		setupUTF8Console();
#endif

		// Install signal handlers
		std::signal(SIGINT, signalHandler);
#ifndef _WIN32
		std::signal(SIGTERM, signalHandler);
#endif

		// Parse command line arguments
		auto config = parseArgs(argc, argv);

		if (config.show_help) {
			printUsage(argv[0]);
			return 0;
		}

		// Print header and environment info
		benchmark::MainBenchmark::printHeader();
		printEnvironmentInfo();

		// Validate system requirements
		std::cout << "\n🔍 SYSTEM VALIDATION:\n";
		std::cout << std::string(50, 0x2D) << "\n";

		// Check for minimum hardware requirements
		auto hw_threads = std::thread::hardware_concurrency();
		if (hw_threads < 2) {
			std::cout << "⚠️  Warning: Only " << hw_threads << " hardware thread(s) detected.\n";
			std::cout << "   Multi-threaded benchmarks may not be meaningful.\n";
		} else {
			std::cout << "✓ Hardware threads: " << hw_threads << "\n";
		}

		// Check object size
		if constexpr (sizeof(benchmark::LargeTestObject) < 65535) {
			std::cerr << "❌ Error: LargeTestObject is too small ("
					  << sizeof(benchmark::LargeTestObject) << " bytes)\n";
			return 1;
		} else {
			std::cout << "✓ Test object size: " << sizeof(benchmark::LargeTestObject) << " bytes\n";
		}

		// Memory availability check (rough estimate)
		size_t estimated_memory_mb = (sizeof(benchmark::LargeTestObject) * 2048) / (static_cast<unsigned long long>(1024) * 1024);
		std::cout << "✓ Estimated memory usage: ~" << estimated_memory_mb << " MB\n";

		std::cout << "✓ System validation passed\n";

		// Print configuration
		std::cout << "\n⚙️  BENCHMARK CONFIGURATION:\n";
		std::cout << std::string(50, 0x2D) << "\n";
		std::cout << "Mode: ";
		switch (config.mode) {
			case BenchmarkConfig::COMPLETE:
				std::cout << "Complete Suite\n";
				break;
			case BenchmarkConfig::LIGHT:
				std::cout << "Lightweight\n";
				break;
			case BenchmarkConfig::INTEGRATION:
				std::cout << "Integration Test\n";
				break;
			case BenchmarkConfig::BASELINE:
				std::cout << "Baseline Only\n";
				break;
			case BenchmarkConfig::POOL:
				std::cout << "Pool Only\n";
				break;
			case BenchmarkConfig::MULTITHREAD:
				std::cout << "Multi-threaded Only\n";
				break;
			case BenchmarkConfig::STRESS:
				std::cout << "Stress Test Only\n";
				break;
			case BenchmarkConfig::ANALYSIS:
				std::cout << "Analysis Only\n";
				break;
			case BenchmarkConfig::THREADID:
				std::cout << "ThreadId Optimization Tests\n";
				break;
		}

		if (config.ops > 0) {
			std::cout << "Operations: " << config.ops << "\n";
		}
		if (config.max_threads > 0) {
			std::cout << "Max threads: " << config.max_threads << "\n";
		}
		std::cout << "Warmup operations: " << config.warmup_ops << "\n";
		std::cout << std::string(50, 0x2D) << "\n";

		// Run the benchmarks
		auto start_time = std::chrono::high_resolution_clock::now();

		runBenchmarkModule(config);

		auto end_time = std::chrono::high_resolution_clock::now();
		auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
								  end_time - start_time
		)
								  .count();

		// Print completion summary
		if (!g_shutdown_requested) {
			std::cout << "\n📊 EXECUTION SUMMARY:\n";
			std::cout << std::string(50, 0x2D) << "\n";
			std::cout << "Total execution time: " << (static_cast<double>(total_duration) / 1000.0) << " seconds\n";
			std::cout << "Peak memory object: ~" << sizeof(benchmark::LargeTestObject) << " bytes\n";
			std::cout << "Status: ✅ COMPLETED SUCCESSFULLY\n";

			benchmark::MainBenchmark::printFooter();
		} else {
			std::cout << "\n⚠️  Benchmark interrupted by user signal.\n";
			std::cout << "Partial results may be available above.\n";
			return 130; // Standard exit code for SIGINT
		}

		return 0;

	} catch (const std::exception &e) {
		std::cerr << "\n❌ FATAL ERROR: " << e.what() << "\n";
		std::cerr << "Benchmark execution failed.\n";
		return 1;
	} catch (...) {
		std::cerr << "\n❌ UNKNOWN FATAL ERROR occurred during benchmark execution.\n";
		return 1;
	}
}

/**
 * @brief Additional utility functions for advanced usage
 */
namespace benchmark_utils {

	/**
	 * @brief Run custom benchmark with specific parameters
	 */
	[[maybe_unused]] static void runCustomBenchmark(size_t pool_size, size_t cache_size, size_t ops, size_t threads = 1) {
		std::cout << "\n🔧 CUSTOM BENCHMARK:\n";
		std::cout << "Pool Size: " << pool_size << ", Cache Size: " << cache_size
				  << ", Ops: " << ops << ", Threads: " << threads << "\n";
		std::cout << std::string(50, 0x2D) << "\n";

		// Implementation would depend on the specific pool configuration
		// This is a template for custom benchmarks

		std::cout << "✓ Custom benchmark would run here\n";
	}

	/**
	 * @brief Quick performance validation
	 */
	[[maybe_unused]] static bool validatePerformance() {
		using namespace benchmark;

		std::cout << "\n🚀 QUICK PERFORMANCE VALIDATION:\n";
		std::cout << std::string(50, 0x2D) << "\n";

		// Quick 1000-operation test
		using TestPool = SharedOptimizedObjectPool<LargeTestObject, 128, true>;
		TestPool pool;
		pool.prewarm(32);

		auto start = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < 1000; ++i) {
			auto obj = pool.acquire().value();
			if (obj) {
				obj->writeByte(42);
			}
		}
		auto end = std::chrono::high_resolution_clock::now();

		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		const double ops_per_sec = 1000.0 * 1000000.0 / static_cast<double>(duration);

		std::cout << "1000 operations completed in " << duration << " microseconds\n";
		std::cout << "Performance: " << static_cast<size_t>(ops_per_sec) << " ops/sec\n";

		// Simple validation threshold
		const bool passed = ops_per_sec > 100000; // At least 100K ops/sec
		std::cout << "Validation: " << (passed ? "✅ PASSED" : "❌ FAILED") << "\n";

		return passed;
	}
}

// Conditional compilation for testing frameworks
#ifdef ENABLE_TESTING_INTEGRATION
	#include <gtest/gtest.h>

static TEST(ObjectPoolBenchmark, QuickValidation) {
	EXPECT_TRUE(benchmark_utils::validatePerformance());
}

static TEST(ObjectPoolBenchmark, IntegrationTest) {
	EXPECT_NO_THROW(benchmark::MainBenchmark::runQuickIntegrationTest());
}
#endif
