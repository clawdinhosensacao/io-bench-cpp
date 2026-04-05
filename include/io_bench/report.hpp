#pragma once

#include "io_bench/types.hpp"
#include <vector>
#include <string>
#include <filesystem>

namespace io_bench {

/// Generate markdown report from benchmark results
std::string generate_markdown_report(const std::vector<BenchResult>& results, 
                                     const BenchConfig& config);

/// Write report to file
void write_report(const std::string& path, const std::string& content);

/// Print results to console
void print_results(const std::vector<BenchResult>& results);

/// Get current timestamp as ISO string
std::string current_timestamp();

} // namespace io_bench
