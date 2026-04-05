#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <optional>

namespace io_bench {

/// Result of a single benchmark run
struct BenchResult {
    std::string name;              ///< Format name
    bool available = false;        ///< Whether the format backend is available
    double file_size_mb = 0.0;     ///< File size in megabytes
    double write_ms = 0.0;         ///< Write time in milliseconds
    double read_ms = 0.0;          ///< Read time in milliseconds
    double write_mbps = 0.0;       ///< Write throughput in MB/s
    double read_mbps = 0.0;        ///< Read throughput in MB/s
    std::string error;             ///< Error message if failed
};

/// Configuration for benchmark run
struct BenchConfig {
    std::size_t nx = 100;          ///< Grid size X
    std::size_t nz = 80;           ///< Grid size Z
    std::size_t ny = 1;            ///< Grid size Y (for 3D, default 1)
    std::size_t iterations = 1;    ///< Number of iterations
    std::string output_dir = ".";  ///< Output directory for test files
    bool warmup = true;            ///< Run warmup iteration before timing
};

/// Array shape descriptor
struct ArrayShape {
    std::size_t nx;
    std::size_t nz;
    std::size_t ny = 1;  // For 3D arrays
    
    std::size_t total() const { return nx * nz * ny; }
    std::size_t bytes() const { return total() * sizeof(float); }
    double mb() const { return static_cast<double>(bytes()) / (1024.0 * 1024.0); }
    
    bool is_3d() const { return ny > 1; }
};

/// Timer helper class
class Timer {
public:
    void start() { start_ = std::chrono::high_resolution_clock::now(); }
    
    void stop() { end_ = std::chrono::high_resolution_clock::now(); }
    
    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(end_ - start_).count();
    }
    
    double elapsed_s() const {
        return std::chrono::duration<double>(end_ - start_).count();
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::high_resolution_clock::time_point end_;
};

/// Format adapter interface
class FormatAdapter {
public:
    virtual ~FormatAdapter() = default;
    virtual std::string name() const = 0;
    virtual bool is_available() const = 0;
    virtual void write(const std::string& path, const float* data, const ArrayShape& shape) = 0;
    virtual void read(const std::string& path, float* data, const ArrayShape& shape) = 0;
    virtual std::string extension() const = 0;
};

/// Calculate throughput
inline double throughput_mbps(double size_mb, double seconds) {
    if (seconds <= 0.0) return 0.0;
    return size_mb / seconds;
}

} // namespace io_bench
