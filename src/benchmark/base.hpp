#pragma once

#include "../include/lockfree/lockfree.hpp"
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <memory_resource>
#include <numeric>
#include <cmath>
#include <map>
#include <string_view>
#include <atomic>
#include <algorithm>

namespace benchmark {

	class LargeTestObject {
	public:
		static constexpr size_t BUFFER_SIZE = 65535 - sizeof(size_t) - sizeof(uint32_t) - sizeof(int16_t);

		LargeTestObject() noexcept :
			m_size(0), m_checksum(0), threadId(-1) { }

		~LargeTestObject() = default;

		void reset() noexcept {
			m_size = 0;
			m_checksum = 0;
			// Don't reset threadId as it's managed by the pool
		}

		void writeData(const void* data, size_t len) noexcept {
			if (m_size + len > BUFFER_SIZE) {
				return;
			}
			std::memcpy(m_buffer + m_size, data, len);
			m_size += len;
			const uint8_t* bytes = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < len; ++i) {
				m_checksum ^= bytes[i];
			}
		}

		void writeByte(uint8_t byte) noexcept {
			if (m_size < BUFFER_SIZE) {
				m_buffer[m_size++] = byte;
				m_checksum ^= byte;
			}
		}

		void writeUInt32(uint32_t value) noexcept {
			writeData(&value, sizeof(value));
		}

		void writeString(const char* str) noexcept {
			size_t len = std::strlen(str);
			writeUInt32(static_cast<uint32_t>(len));
			writeData(str, len);
		}

		size_t getSize() const noexcept { return m_size; }
		uint32_t getChecksum() const noexcept { return m_checksum; }

		// Thread ID support for the pool
		int16_t threadId;

	private:
		uint8_t m_buffer[BUFFER_SIZE];
		size_t m_size;
		uint32_t m_checksum;
	};

	static_assert(sizeof(LargeTestObject) >= 65535, "LargeTestObject must be at least 65535 bytes");

	struct BenchmarkResult {
		std::string name;
		double avg_time_ms = 0.0;
		double min_time_ms = 0.0;
		double max_time_ms = 0.0;
		double stddev_ms = 0.0;
		size_t operations = 0;
		double ops_per_sec = 0.0;
		double memory_mb = 0.0;
		double speedup = 1.0;

		// Extended metrics
		double cache_hit_rate = 0.0;
		double cross_thread_ratio = 0.0;
		size_t objects_in_use = 0;
		size_t pool_creates = 0;
	};

	// Global baselines for speedup calculations
	extern double g_st_baseline_avg_ms;
	extern std::map<size_t, double> g_mt_baseline_avg_ms;

	/**
	 * @brief Base benchmark utilities
	 */
	class BenchmarkBase {
	public:
		using Clock = std::chrono::high_resolution_clock;
		using Duration = std::chrono::duration<double, std::milli>;

		/**
		 * @brief Calculate statistics from timing data
		 */
		static BenchmarkResult calculateStats(const std::string &name,
											const std::vector<double> &times,
											size_t total_ops,
											double baseline_avg_ms = 0.0) {
			if (times.empty()) {
				return { name, 0, 0, 0, 0, total_ops, 0, 0, 0 };
			}

			double sum = std::accumulate(times.begin(), times.end(), 0.0);
			double min = *std::min_element(times.begin(), times.end());
			double max = *std::max_element(times.begin(), times.end());
			double avg = sum / times.size();

			double variance = 0;
			for (double t : times) {
				variance += (t - avg) * (t - avg);
			}
			variance /= times.size();
			double stddev = std::sqrt(variance);

			double ops_per_sec = (avg > 0) ? (total_ops * 1000.0) / avg : 0.0;
			double memory_mb = (sizeof(LargeTestObject) * total_ops) / (1024.0 * 1024.0);
			double speedup = (baseline_avg_ms > 0 && avg > 0) ? baseline_avg_ms / avg : 1.0;

			return { name, avg, min, max, stddev, total_ops, ops_per_sec, memory_mb, speedup };
		}

		/**
		 * @brief Enhanced result printing
		 */
		static void printResult(const BenchmarkResult &r) {
			std::cout << std::fixed << std::setprecision(3);
			std::cout << std::left << std::setw(40) << r.name
					  << " | Avg: " << std::right << std::setw(8) << r.avg_time_ms << " ms"
					  << " | Ops/s: " << std::setw(12) << static_cast<size_t>(r.ops_per_sec)
					  << " | Speedup: " << std::setw(6) << r.speedup << "x";

			if (r.cache_hit_rate > 0) {
				std::cout << " | Cache: " << std::setw(5) << std::setprecision(1) << r.cache_hit_rate << "%";
			}

			if (r.cross_thread_ratio > 0) {
				std::cout << " | X-Thread: " << std::setw(5) << std::setprecision(1) << r.cross_thread_ratio << "%";
			}

			std::cout << "\n";

			if (r.objects_in_use > 0 || r.pool_creates > 0) {
				std::cout << std::string(40, ' ') << " | InUse: " << r.objects_in_use
						  << " | Creates: " << r.pool_creates << "\n";
			}
		}

		/**
		 * @brief Enhanced result printing with additional metrics
		 */
		static void printDetailedResult(const BenchmarkResult &r) {
			printResult(r);
			if (r.stddev_ms > 0) {
				std::cout << std::string(40, ' ') << " | Min: " << std::setw(8) << r.min_time_ms
						  << " | Max: " << std::setw(8) << r.max_time_ms
						  << " | StdDev: " << std::setw(8) << r.stddev_ms << "\n";
			}
		}

		/**
		 * @brief System warmup
		 */
		static void warmup(size_t ops = 1000) {
			std::cout << "🔥 Warming up system...\n";

			std::vector<std::unique_ptr<LargeTestObject>> objects;
			objects.reserve(ops);

			for (size_t i = 0; i < ops; ++i) {
				objects.emplace_back(std::make_unique<LargeTestObject>());
				objects.back()->writeString("warmup");
				objects.back()->writeUInt32(static_cast<uint32_t>(i));
			}

			// Access pattern to warm caches
			for (auto& obj : objects) {
				volatile auto checksum = obj->getChecksum();
				(void)checksum;
			}

			std::cout << "✓ Warmup complete\n\n";
		}

		/**
		 * @brief Print section header
		 */
		static void printSectionHeader(const std::string& title, int section_num = 0) {
			std::cout << "\n";
			if (section_num > 0) {
				std::cout << section_num << "️⃣ ";
			}
			std::cout << title << "\n";
			std::cout << std::string(90, '═') << "\n";
		}

		/**
		 * @brief Print subsection header
		 */
		static void printSubsectionHeader(const std::string& title) {
			std::cout << "\n" << title << "\n";
			std::cout << std::string(60, '─') << "\n";
		}

		/**
		 * @brief Validate object properties
		 */
		static void validateObjectSize() {
			std::cout << "\n📏 Object Size & Alignment Validation:\n";
			std::cout << std::string(50, '─') << "\n";
			std::cout << "  sizeof(LargeTestObject): " << sizeof(LargeTestObject) << " bytes\n";
			std::cout << "  Target size: ≥65535 bytes\n";
			std::cout << "  Overhead: " << sizeof(LargeTestObject) - 65535 << " bytes\n";
			std::cout << "  alignof(LargeTestObject): " << alignof(LargeTestObject) << " bytes\n";
			std::cout << "  CACHE_LINE_SIZE: " << CACHE_LINE_SIZE << " bytes\n";

			// Validate threadId field
			LargeTestObject test_obj;
			std::cout << "  threadId field offset: "
					  << reinterpret_cast<uintptr_t>(&test_obj.threadId) - reinterpret_cast<uintptr_t>(&test_obj)
					  << " bytes\n";
		}

		/**
		 * @brief Print system information
		 */
		static void printSystemInfo() {
			std::cout << "\n📊 SYSTEM INFORMATION:\n";
			std::cout << "├─ Hardware Threads: " << std::thread::hardware_concurrency() << "\n";
			std::cout << "├─ Cache Line Size: " << CACHE_LINE_SIZE << " bytes\n";
			std::cout << "├─ Object Size: " << sizeof(LargeTestObject) << " bytes (~"
					  << (sizeof(LargeTestObject) / 1024) << " KB)\n";
			std::cout << "└─ Test Object Features: ThreadId support, Reset capability\n\n";
		}

		/**
		 * @brief Generate thread count list for testing
		 */
		static std::vector<size_t> generateThreadCounts(size_t max_threads = 0) {
			if (max_threads == 0) {
				max_threads = std::min(std::thread::hardware_concurrency() * 2, 32u);
			}

			std::vector<size_t> thread_counts;
			for (size_t t = 1; t <= max_threads; t *= 2) {
				thread_counts.push_back(t);
			}
			if (thread_counts.back() != max_threads && max_threads <= 32) {
				thread_counts.push_back(max_threads);
			}

			return thread_counts;
		}

		/**
		 * @brief Extract and add pool statistics to result
		 */
		template<typename PoolType>
		static void addPoolStats(BenchmarkResult& result, const PoolType& pool) {
			if constexpr (requires { pool.get_stats(); }) {
				auto stats = pool.get_stats();
				result.cache_hit_rate = stats.acquires > 0 ? (stats.same_thread_hits * 100.0 / stats.acquires) : 0.0;
				result.cross_thread_ratio = stats.acquires > 0 ? (stats.cross_thread_ops * 100.0 / stats.acquires) : 0.0;
				result.objects_in_use = stats.in_use;
				result.pool_creates = stats.creates;
			}
		}
	};

} // namespace benchmark