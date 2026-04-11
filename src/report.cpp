#include "io_bench/report.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace io_bench {

std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};  // NOLINT(misc-include-cleaner) — std::gmtime_r not in <ctime>
#ifdef __linux__
    gmtime_r(&time, &tm_buf);
#else
    tm_buf = *std::gmtime(&time);  // NOLINT(concurrency-mt-unsafe)
#endif
    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

void write_report(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot write report: " + path);
    }
    file << content;
}

void print_results(const std::vector<BenchResult>& results) {
    std::cout << "\n";
    std::cout << "| Format | Status | Size (MB) | Ratio | Write (ms) | Read (ms) | Write MB/s | Read MB/s | Peak RSS (MB) |\n";
    std::cout << "|--------|--------|-----------|-------|------------|-----------|------------|-----------|---------------|\n";
    
    for (const auto& r : results) {
        std::cout << "| " << std::setw(12) << std::left << r.name << " | ";
        std::cout << (r.available ? "OK" : "N/A") << " | ";
        
        if (r.available && r.error.empty()) {
            std::cout << std::fixed << std::setprecision(3) << r.file_size_mb << " | ";
            std::cout << std::setprecision(2) << r.compression_ratio << ":1 | ";
            std::cout << std::setprecision(2) << r.write_ms << " | ";
            std::cout << r.read_ms << " | ";
            std::cout << std::setprecision(1) << r.write_mbps << " | ";
            std::cout << r.read_mbps << " | ";
            std::cout << std::setprecision(1) << std::max(r.peak_write_rss_mb, r.peak_read_rss_mb) << " |";
        } else if (r.available && !r.error.empty()) {
            std::cout << " - | - | - | - | - | - | - | " << r.error << " |";
        } else {
            std::cout << " - | - | - | - | - | - | - |";
        }
        std::cout << "\n";
    }
}

std::string generate_markdown_report(const std::vector<BenchResult>& results, 
                                     const BenchConfig& config) {
    std::stringstream ss;
    
    ss << "# I/O Format Benchmark Report (C++)\n\n";
    ss << "**Generated:** " << current_timestamp() << "\n\n";
    
    ss << "## Configuration\n\n";
    ss << "- Grid size: " << config.nx << " x " << config.nz << "\n";
    ss << "- Iterations: " << config.iterations << "\n";
    ss << "- Data type: float32\n\n";
    
    // Count available/unavailable
    std::size_t available = 0;
    for (const auto& r : results) {
        if (r.available) { ++available;
}
    }
    ss << "**Available formats:** " << available << "/" << results.size() << "\n\n";
    
    ss << "## Results\n\n";
    ss << "| Format | Status | Size (MB) | Ratio | Write (ms) | Read (ms) | Write MB/s | Read MB/s | Peak RSS (MB) | Notes |\n";
    ss << "|--------|--------|-----------|-------|------------|-----------|------------|-----------|---------------|-------|\n";
    
    for (const auto& r : results) {
        ss << "| " << r.name << " | ";
        ss << (r.available ? "✅" : "❌") << " | ";
        
        if (!r.available) {
            ss << "n/a | n/a | n/a | n/a | n/a | n/a | n/a | not compiled |";
        } else if (!r.error.empty()) {
            ss << " - | - | - | - | - | - | - | " << r.error << " |";
        } else {
            ss << std::fixed << std::setprecision(3) << r.file_size_mb << " | ";
            ss << std::setprecision(2) << r.compression_ratio << ":1 | ";
            ss << std::setprecision(2) << r.write_ms << " | ";
            ss << r.read_ms << " | ";
            ss << std::setprecision(1) << r.write_mbps << " | ";
            ss << r.read_mbps << " | ";
            ss << std::setprecision(1) << std::max(r.peak_write_rss_mb, r.peak_read_rss_mb) << " | - |";
        }
        ss << "\n";
    }
    
    // Top rankings
    ss << "\n## Rankings\n\n";
    
    // Top read throughput
    std::vector<const BenchResult*> sorted_read;
    for (const auto& r : results) {
        if (r.available && r.error.empty() && r.read_mbps > 0) {
            sorted_read.push_back(&r);
        }
    }
    std::ranges::sort(sorted_read, 
              [](const BenchResult* a, const BenchResult* b) {
                  return a->read_mbps > b->read_mbps;
              });
    
    ss << "### Top Read Throughput\n\n";
    for (std::size_t i = 0; i < std::min(sorted_read.size(), static_cast<size_t>(3)); ++i) {
        ss << (i + 1) << ". **" << sorted_read[i]->name << "**: " 
           << std::fixed << std::setprecision(1) << sorted_read[i]->read_mbps << " MB/s\n";
    }
    
    // Top write throughput
    std::vector<const BenchResult*> sorted_write;
    for (const auto& r : results) {
        if (r.available && r.error.empty() && r.write_mbps > 0) {
            sorted_write.push_back(&r);
        }
    }
    std::ranges::sort(sorted_write, 
              [](const BenchResult* a, const BenchResult* b) {
                  return a->write_mbps > b->write_mbps;
              });
    
    ss << "\n### Top Write Throughput\n\n";
    for (std::size_t i = 0; i < std::min(sorted_write.size(), static_cast<size_t>(3)); ++i) {
        ss << (i + 1) << ". **" << sorted_write[i]->name << "**: " 
           << std::fixed << std::setprecision(1) << sorted_write[i]->write_mbps << " MB/s\n";
    }
    
    // Top compression ratio
    std::vector<const BenchResult*> sorted_compression;
    for (const auto& r : results) {
        if (r.available && r.error.empty() && r.compression_ratio > 1.0) {
            sorted_compression.push_back(&r);
        }
    }
    std::ranges::sort(sorted_compression, 
              [](const BenchResult* a, const BenchResult* b) {
                  return a->compression_ratio > b->compression_ratio;
              });
    
    if (!sorted_compression.empty()) {
        ss << "\n### Top Compression Ratio\n\n";
        for (std::size_t i = 0; i < std::min(sorted_compression.size(), static_cast<size_t>(5)); ++i) {
            ss << (i + 1) << ". **" << sorted_compression[i]->name << "**: " 
               << std::fixed << std::setprecision(2) << sorted_compression[i]->compression_ratio << ":1 ("
               << std::setprecision(1) << sorted_compression[i]->raw_data_mb << " → "
               << sorted_compression[i]->file_size_mb << " MB)\n";
        }
    }
    
    ss << "\n## Recommendations\n\n";
    ss << "- **Maximum throughput:** Use `binary_f32` or `mmap` for hot paths\n";
    ss << "- **NumPy compatibility:** Use `npy` format\n";
    ss << "- **Self-describing:** Use `hdf5` or `netcdf` for metadata support\n";
    ss << "- **HPC workflows:** Consider `adios2` for parallel I/O\n";
    ss << "- **Space efficiency:** Check compression ratio column — compressed formats reduce storage at throughput cost\n";
    ss << "- **Geophysics inline access:** Use `--slice-read` to compare chunked vs sequential access patterns\n";
    
    return ss.str();
}

} // namespace io_bench
