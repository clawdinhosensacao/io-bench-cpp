#pragma once

#include "io_bench/types.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <filesystem>

namespace io_bench {

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(const BenchConfig& config);
    ~BenchmarkRunner();
    
    /// Add a format adapter
    void add_format(std::unique_ptr<FormatAdapter> adapter);
    
    /// Run all benchmarks
    [[nodiscard]] std::vector<BenchResult> run_all();
    
    /// Run benchmark for a single format
    [[nodiscard]] BenchResult run_single(FormatAdapter& adapter);
    
    /// Generate test data
    [[nodiscard]] std::vector<float> generate_data(const ArrayShape& shape, unsigned int seed = 0);
    
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
