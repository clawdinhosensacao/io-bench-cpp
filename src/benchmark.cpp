#include "io_bench/benchmark.hpp"
#include "io_bench/formats.hpp"
#include <random>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <utility>

namespace io_bench {

BenchmarkRunner::BenchmarkRunner(BenchConfig  config) 
    : config_(std::move(config)) {
    // Create temp directory for benchmark files
    temp_dir_ = std::filesystem::temp_directory_path() / "io_bench_cpp";
    std::filesystem::create_directories(temp_dir_);
}

BenchmarkRunner::~BenchmarkRunner() {
    // Clean up temp files
    try {
        std::filesystem::remove_all(temp_dir_);
    } catch (...) {
        // Ignore cleanup errors
    }
}

void BenchmarkRunner::add_format(std::unique_ptr<FormatAdapter> adapter) {
    adapters_.push_back(std::move(adapter));
}

std::vector<float> BenchmarkRunner::generate_data(const ArrayShape& shape, unsigned int seed) {
    std::vector<float> data(shape.total());
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(1000.0f, 5000.0f);
    
    for (auto& v : data) {
        v = dist(rng);
    }
    
    return data;
}

BenchResult BenchmarkRunner::run_single(FormatAdapter& adapter) {
    BenchResult result;
    result.name = adapter.name();
    result.available = adapter.is_available();
    
    if (!result.available) {
        return result;
    }
    
    ArrayShape shape{.nx=config_.nx, .nz=config_.nz, .ny=config_.ny};
    auto data = generate_data(shape);
    std::vector<float> read_buffer(shape.total());
    
    // Generate temp file path
    auto file_path = temp_dir_ / (adapter.name() + adapter.extension());
    std::string path_str = file_path.string();
    
    try {
        // Warmup iteration
        if (config_.warmup) {
            adapter.write(path_str, data.data(), shape);
            adapter.read(path_str, read_buffer.data(), shape);
        }
        
        // Timed iterations
        double total_write_ms = 0.0;
        double total_read_ms = 0.0;
        
        for (std::size_t i = 0; i < config_.iterations; ++i) {
            // Write
            Timer write_timer;
            write_timer.start();
            adapter.write(path_str, data.data(), shape);
            write_timer.stop();
            total_write_ms += write_timer.elapsed_ms();
            
            // Get file size after write (handles both files and directories)
            if (std::filesystem::is_directory(file_path)) {
                std::uintmax_t dir_size = 0;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(file_path)) {
                    if (std::filesystem::is_regular_file(entry)) {
                        dir_size += std::filesystem::file_size(entry);
                    }
                }
                result.file_size_mb = static_cast<double>(dir_size) / (1024.0 * 1024.0);
            } else {
                result.file_size_mb = std::filesystem::file_size(file_path) / (1024.0 * 1024.0);
            }
            
            // Read
            Timer read_timer;
            read_timer.start();
            adapter.read(path_str, read_buffer.data(), shape);
            read_timer.stop();
            total_read_ms += read_timer.elapsed_ms();
        }
        
        // Average times
        result.write_ms = total_write_ms / config_.iterations;
        result.read_ms = total_read_ms / config_.iterations;
        
        // Calculate throughput
        double write_s = result.write_ms / 1000.0;
        double read_s = result.read_ms / 1000.0;
        result.write_mbps = throughput_mbps(result.file_size_mb, write_s);
        result.read_mbps = throughput_mbps(result.file_size_mb, read_s);
        
        // Verify data integrity (first iteration only)
        for (std::size_t i = 0; i < shape.total(); ++i) {
            if (std::abs(data[i] - read_buffer[i]) > 0.001f) {
                result.error = "Data integrity check failed";
                break;
            }
        }
        
        // Cleanup (handles both files and directories)
        std::filesystem::remove_all(file_path);
        
    } catch (const std::exception& e) {
        result.error = e.what();
    }
    
    return result;
}

std::vector<BenchResult> BenchmarkRunner::run_all() {
    std::vector<BenchResult> results;
    
    results.reserve(adapters_.size());
for (auto& adapter : adapters_) {
        results.push_back(run_single(*adapter));
    }
    
    return results;
}

std::vector<std::unique_ptr<FormatAdapter>> create_all_adapters() {
    std::vector<std::unique_ptr<FormatAdapter>> adapters;
    
    // Always available
    adapters.push_back(std::make_unique<BinaryFormat>());
    adapters.push_back(std::make_unique<BinaryHeaderFormat>());
    adapters.push_back(std::make_unique<MmapFormat>());
    adapters.push_back(std::make_unique<NpyFormat>());
    adapters.push_back(std::make_unique<JsonFormat>());
    
    // Optional formats
    adapters.push_back(std::make_unique<Hdf5Format>());
    adapters.push_back(std::make_unique<NetcdfFormat>());
    adapters.push_back(std::make_unique<TileDBFormat>());
    adapters.push_back(std::make_unique<Adios2Format>());
    
    // Future formats (placeholders)
    adapters.push_back(std::make_unique<ZarrFormat>());
    adapters.push_back(std::make_unique<ParquetFormat>());
    adapters.push_back(std::make_unique<SegyFormat>());
    adapters.push_back(std::make_unique<TensorStoreFormat>());
    adapters.push_back(std::make_unique<MdioFormat>());
    adapters.push_back(std::make_unique<MiniSeedFormat>());
    adapters.push_back(std::make_unique<RsfFormat>());
    adapters.push_back(std::make_unique<AsdfFormat>());
    adapters.push_back(std::make_unique<SegDFormat>());
    
    return adapters;
}

} // namespace io_bench
