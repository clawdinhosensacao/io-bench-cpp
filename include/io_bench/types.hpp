#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>
#include <optional>
#include <filesystem>

namespace io_bench {

/// Result of a single benchmark run
struct BenchResult {
    std::string name;              ///< Format name
    bool available = false;        ///< Whether the format backend is available
    double file_size_mb = 0.0;     ///< File size in megabytes
    double raw_data_mb = 0.0;     ///< Raw data size in megabytes (for compression ratio)
    double write_ms = 0.0;         ///< Write time in milliseconds
    double read_ms = 0.0;          ///< Read time in milliseconds
    double write_mbps = 0.0;       ///< Write throughput in MB/s
    double read_mbps = 0.0;        ///< Read throughput in MB/s
    double compression_ratio = 0.0; ///< raw_data_mb / file_size_mb (1.0 = no compression)
    double peak_write_rss_mb = 0.0; ///< Peak RSS during write (MB)
    double peak_read_rss_mb = 0.0;  ///< Peak RSS during read (MB)
    std::string error;             ///< Error message if failed
};

/// Configuration for benchmark run
struct BenchConfig {
    std::size_t nx = 100;          ///< Grid size X
    std::size_t nz = 80;           ///< Grid size Z
    std::size_t ny = 1;            ///< Grid size Y (for 3D, default 1)
    std::size_t iterations = 1;    ///< Number of iterations
    std::size_t parallel_threads = 1; ///< Number of concurrent read threads (1 = sequential)
    std::string output_dir = ".";  ///< Output directory for test files
    bool warmup = true;            ///< Run warmup iteration before timing
};

/// Array shape descriptor
struct ArrayShape {
    std::size_t nx;
    std::size_t nz;
    std::size_t ny = 1;  // For 3D arrays
    
    [[nodiscard]] std::size_t total() const { return nx * nz * ny; }
    [[nodiscard]] std::size_t bytes() const { return total() * sizeof(float); }
    [[nodiscard]] double mb() const { return static_cast<double>(bytes()) / (1024.0 * 1024.0); }
    
    [[nodiscard]] bool is_3d() const { return ny > 1; }
};

/// Timer helper class
class Timer {
public:
    void start() { start_ = std::chrono::high_resolution_clock::now(); }
    
    void stop() { end_ = std::chrono::high_resolution_clock::now(); }
    
    [[nodiscard]] double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(end_ - start_).count();
    }
    
    [[nodiscard]] double elapsed_s() const {
        return std::chrono::duration<double>(end_ - start_).count();
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::high_resolution_clock::time_point end_;
};

/// Result of a slice read benchmark
struct SliceReadResult {
    std::string name;
    bool available = false;
    bool supports_slice = false;
    double file_size_mb = 0.0;       ///< Total volume file size (MB)
    double slice_size_mb = 0.0;      ///< Single inline slice size (MB)
    double slice_read_ms = 0.0;     ///< Time to read one inline slice (ms)
    double slice_read_mbps = 0.0;   ///< Throughput for slice read (MB/s)
    double full_read_ms = 0.0;      ///< Time to read full volume (ms, for comparison)
    double full_read_mbps = 0.0;    ///< Throughput for full volume read (MB/s)
    double speedup = 0.0;           ///< full_read_ms / slice_read_ms (ideal = ny)
    std::string error;
};

/// Result of a trace read benchmark (SEG-Y trace-based access pattern)
struct TraceReadResult {
    std::string name;
    bool available = false;
    double file_size_mb = 0.0;       ///< Total file size (MB)
    std::size_t num_traces = 0;      ///< Number of traces in the file
    std::size_t samples_per_trace = 0;  ///< Samples per trace
    double trace_size_kb = 0.0;      ///< Single trace size (KB)
    double sequential_read_ms = 0.0; ///< Time to read all traces sequentially (ms)
    double sequential_mbps = 0.0;    ///< Sequential read throughput (MB/s)
    double random_read_ms = 0.0;     ///< Time to read N random traces (ms)
    double random_mbps = 0.0;        ///< Random trace read throughput (MB/s)
    double random_traces_read = 0.0; ///< Number of traces read in random test
    std::string error;
};

/// Result of a streaming write benchmark (append-only trace writes)
struct StreamWriteResult {
    std::string name;
    bool available = false;
    std::size_t num_traces = 0;          ///< Number of traces written
    std::size_t samples_per_trace = 0;  ///< Samples per trace
    double trace_size_kb = 0.0;         ///< Single trace size (KB)
    double stream_write_ms = 0.0;       ///< Total time for streaming write (ms)
    double stream_write_mbps = 0.0;     ///< Streaming write throughput (MB/s)
    double bulk_write_ms = 0.0;         ///< Bulk write time for comparison (ms)
    double bulk_write_mbps = 0.0;       ///< Bulk write throughput (MB/s)
    double slowdown = 0.0;              ///< stream_write_ms / bulk_write_ms (>1 = streaming slower)
    std::string error;
};

/// Result of a checkpoint/restart benchmark (write-then-read-back round trip)
struct CheckpointResult {
    std::string name;
    bool available = false;
    double file_size_mb = 0.0;          ///< File size (MB)
    double write_ms = 0.0;              ///< Checkpoint write time (ms)
    double read_ms = 0.0;              ///< Restart read time (ms)
    double round_trip_ms = 0.0;        ///< Total write+read time (ms)
    double write_mbps = 0.0;           ///< Write throughput (MB/s)
    double read_mbps = 0.0;           ///< Read throughput (MB/s)
    bool integrity_ok = false;          ///< Whether read-back data matches written data
    double max_abs_error = 0.0;        ///< Maximum absolute error between written and read data
    std::string error;
};

/// Single data point in a compression level sweep
struct CompressionLevelResult {
    int level = 0;                     ///< Compression level (0=none, 1-9=fastest..best)
    double file_size_mb = 0.0;         ///< Compressed file size (MB)
    double compression_ratio = 0.0;    ///< raw_data_mb / file_size_mb
    double write_ms = 0.0;             ///< Write time (ms)
    double read_ms = 0.0;             ///< Read time (ms)
    double write_mbps = 0.0;          ///< Write throughput (MB/s)
    double read_mbps = 0.0;          ///< Read throughput (MB/s)
};

/// Result of a compression level sweep benchmark
struct CompressionSweepResult {
    std::string name;                   ///< Format name
    std::string compressor;             ///< Compressor name (e.g. "blosc-lz4", "gzip")
    bool available = false;
    double raw_data_mb = 0.0;          ///< Uncompressed data size (MB)
    std::vector<CompressionLevelResult> levels;  ///< Results per compression level
    std::string error;
};

/// Format adapter interface
class FormatAdapter {
public:
    virtual ~FormatAdapter() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual bool is_available() const = 0;
    virtual void write(const std::string& path, const float* data, const ArrayShape& shape) = 0;
    virtual void read(const std::string& path, float* data, const ArrayShape& shape) = 0;
    [[nodiscard]] virtual std::string extension() const = 0;
    /// Whether this format supports concurrent reads from multiple threads
    [[nodiscard]] virtual bool is_thread_safe() const { return true; }
    
    /// Whether this format supports partial/slice reads without loading entire file
    [[nodiscard]] virtual bool supports_slice_read() const { return false; }
    
    /// Read a single inline (Y-slice) from a 3D volume.
    /// slice_buf must have space for nx * nz floats.
    /// iy is the inline index (0 <= iy < ny).
    /// Default implementation: read full volume and extract slice.
    virtual void read_slice(const std::string& path, float* slice_buf, 
                            const ArrayShape& shape, std::size_t iy) {
        // Fallback: read entire volume and extract the slice
        std::vector<float> full(shape.total());
        read(path, full.data(), shape);
        const std::size_t slice_elements = shape.nx * shape.nz;
        const float* src = full.data() + (iy * slice_elements);
        std::copy(src, src + slice_elements, slice_buf);
    }

    /// Whether this format supports trace-based random access reads
    [[nodiscard]] virtual bool supports_trace_read() const { return false; }

    /// Read a single trace (z-samples at a given trace index).
    /// trace_buf must have space for nz floats.
    /// trace_idx is the trace number (0-based).
    /// Default implementation: read full volume and extract the trace.
    virtual void read_trace(const std::string& path, float* trace_buf,
                            const ArrayShape& shape, std::size_t trace_idx) {
        // Fallback: read entire volume and extract the trace
        std::vector<float> full(shape.total());
        read(path, full.data(), shape);
        const float* src = full.data() + (trace_idx * shape.nz);
        std::copy(src, src + shape.nz, trace_buf);
    }

    /// Whether this format supports streaming (append) trace writes
    [[nodiscard]] virtual bool supports_stream_write() const { return false; }

    /// Write a single trace in streaming/append mode.
    /// trace_buf contains nz samples for one trace.
    /// trace_idx is the 0-based trace number (for formats that need it).
    /// Default: not supported — will fall back to bulk write in benchmark.
    virtual void write_trace(const std::string& /*path*/, const float* /*trace_buf*/,
                            const ArrayShape& /*shape*/, std::size_t /*trace_idx*/) {}

    /// Whether this format supports compression level sweep
    [[nodiscard]] virtual bool supports_compression_sweep() const { return false; }

    /// Get the compressor name for this format (for display in sweep results)
    [[nodiscard]] virtual std::string compressor_name() const { return ""; }

    /// Write with a specific compression level (0 = no compression, 1-9 = increasing)
    /// Default: delegates to write() (no compression control)
    virtual void write_compressed(const std::string& path, const float* data,
                                   const ArrayShape& shape, int level) {
        (void)level;
        write(path, data, shape);
    }
};

/// Calculate throughput
inline double throughput_mbps(double size_mb, double seconds) {
    if (seconds <= 0.0) { return 0.0;
}
    return size_mb / seconds;
}

/// Get current process RSS in MB (from /proc/self/status on Linux)
inline double get_rss_mb() {
#ifdef __linux__
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) { return 0.0; }
    char line[256];
    double rss_kb = 0.0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%lf", &rss_kb);
            break;
        }
    }
    fclose(f);
    return rss_kb / 1024.0;  // KB to MB
#else
    return 0.0;
#endif
}

/// Calculate file or directory size in MB.
/// For directories (e.g. zarr, tiledb), sums all regular files recursively.
/// For regular files, returns the file size.
/// Returns 0.0 if the path does not exist.
inline double file_size_mb(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) { return 0.0; }
    if (std::filesystem::is_directory(path)) {
        std::uintmax_t dir_size = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (std::filesystem::is_regular_file(entry)) {
                dir_size += std::filesystem::file_size(entry);
            }
        }
        return static_cast<double>(dir_size) / (1024.0 * 1024.0);
    }
    return static_cast<double>(std::filesystem::file_size(path)) / (1024.0 * 1024.0);
}

} // namespace io_bench
