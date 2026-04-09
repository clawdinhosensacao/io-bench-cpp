#pragma once

#include "io_bench/types.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <filesystem>
#include <thread>
#include <atomic>

namespace io_bench {

/// Result of a parallel read benchmark
struct ParallelReadResult {
    std::string name;
    bool available = false;
    std::size_t num_threads = 1;
    double total_read_ms = 0.0;      ///< Total wall-clock read time (ms)
    double per_thread_ms = 0.0;      ///< Average per-thread read time (ms)
    double aggregate_mbps = 0.0;     ///< Aggregate throughput across all threads
    double data_size_mb = 0.0;       ///< Total data read (all threads combined)
    std::string error;
};

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(BenchConfig  config);
    ~BenchmarkRunner();
    
    /// Add a format adapter
    void add_format(std::unique_ptr<FormatAdapter> adapter);
    
    /// Run all benchmarks (sequential)
    [[nodiscard]] std::vector<BenchResult> run_all();
    
    /// Run benchmark for a single format
    [[nodiscard]] BenchResult run_single(FormatAdapter& adapter);
    
    /// Run parallel read benchmark for all formats
    [[nodiscard]] std::vector<ParallelReadResult> run_parallel_read_all();
    
    /// Run parallel read benchmark for a single format
    [[nodiscard]] ParallelReadResult run_parallel_read(FormatAdapter& adapter);
    
    /// Generate test data
    [[nodiscard]] static std::vector<float> generate_data(const ArrayShape& shape, unsigned int seed = 0);
    
    /// Get current configuration
    [[nodiscard]] const BenchConfig& config() const { return config_; }
    
private:
    BenchConfig config_;
    std::vector<std::unique_ptr<FormatAdapter>> adapters_;
    std::filesystem::path temp_dir_;
};

/// Factory for creating all known format adapters
std::vector<std::unique_ptr<FormatAdapter>> create_all_adapters();

} // namespace io_bench
