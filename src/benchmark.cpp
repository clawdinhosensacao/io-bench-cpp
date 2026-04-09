#include "io_bench/benchmark.hpp"
#include "io_bench/formats.hpp"
#include <random>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <utility>
#include <thread>
#include <atomic>
#include <barrier>

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
        
        // Calculate compression ratio
        result.raw_data_mb = shape.mb();
        if (result.file_size_mb > 0.0) {
            result.compression_ratio = result.raw_data_mb / result.file_size_mb;
        }
        
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

ParallelReadResult BenchmarkRunner::run_parallel_read(FormatAdapter& adapter) {
    ParallelReadResult result;
    result.name = adapter.name();
    result.available = adapter.is_available();
    result.num_threads = config_.parallel_threads;

    if (!result.available || config_.parallel_threads <= 1) {
        return result;
    }

    if (!adapter.is_thread_safe()) {
        result.error = "Not thread-safe (Python bridge)";
        return result;
    }

    ArrayShape shape{.nx=config_.nx, .nz=config_.nz, .ny=config_.ny};
    auto data = generate_data(shape);
    auto file_path = temp_dir_ / (adapter.name() + adapter.extension());
    std::string path_str = file_path.string();

    try {
        // Write data once (sequential)
        adapter.write(path_str, data.data(), shape);

        // Get file size
        double file_size_mb = 0.0;
        if (std::filesystem::is_directory(file_path)) {
            std::uintmax_t dir_size = 0;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(file_path)) {
                if (std::filesystem::is_regular_file(entry)) {
                    dir_size += std::filesystem::file_size(entry);
                }
            }
            file_size_mb = static_cast<double>(dir_size) / (1024.0 * 1024.0);
        } else {
            file_size_mb = static_cast<double>(std::filesystem::file_size(file_path)) / (1024.0 * 1024.0);
        }

        // Warmup read
        {
            std::vector<float> warmup_buf(shape.total());
            adapter.read(path_str, warmup_buf.data(), shape);
        }

        // Parallel reads: each thread reads the entire file independently
        const std::size_t num_threads = config_.parallel_threads;
        std::vector<double> thread_times(num_threads, 0.0);
        std::vector<std::vector<float>> buffers(num_threads);
        for (std::size_t t = 0; t < num_threads; ++t) {
            buffers[t].resize(shape.total());
        }

        // Barrier for synchronized start
        std::atomic<bool> go{false};

        std::vector<std::thread> threads;
        std::vector<std::string> thread_errors(num_threads);
        for (std::size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                try {
                    // Spin-wait for go signal
                    while (!go.load(std::memory_order_acquire)) {
                        // busy wait
                    }
                    Timer timer;
                    timer.start();
                    adapter.read(path_str, buffers[t].data(), shape);
                    timer.stop();
                    thread_times[t] = timer.elapsed_ms();
                } catch (const std::exception& e) {
                    thread_errors[t] = e.what();
                    thread_times[t] = -1.0;
                } catch (...) {
                    thread_errors[t] = "Unknown exception";
                    thread_times[t] = -1.0;
                }
            });
        }

        // Start all threads simultaneously
        Timer wall_timer;
        wall_timer.start();
        go.store(true, std::memory_order_release);

        for (auto& th : threads) {
            th.join();
        }
        wall_timer.stop();

        // Check for thread errors
        for (std::size_t t = 0; t < num_threads; ++t) {
            if (!thread_errors[t].empty()) {
                result.error = "Thread " + std::to_string(t) + ": " + thread_errors[t];
                std::filesystem::remove_all(file_path);
                return result;
            }
        }

        result.total_read_ms = wall_timer.elapsed_ms();
        result.per_thread_ms = 0.0;
        for (auto t : thread_times) {
            result.per_thread_ms += t;
        }
        result.per_thread_ms /= static_cast<double>(num_threads);

        // Aggregate throughput: all threads reading simultaneously
        result.data_size_mb = file_size_mb * static_cast<double>(num_threads);
        result.aggregate_mbps = throughput_mbps(result.data_size_mb, result.total_read_ms / 1000.0);

        // Verify first thread's data
        for (std::size_t i = 0; i < shape.total(); ++i) {
            if (std::abs(data[i] - buffers[0][i]) > 0.001f) {
                result.error = "Data integrity check failed in parallel read";
                break;
            }
        }

        // Cleanup
        std::filesystem::remove_all(file_path);

    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

std::vector<ParallelReadResult> BenchmarkRunner::run_parallel_read_all() {
    std::vector<ParallelReadResult> results;
    results.reserve(adapters_.size());
    for (auto& adapter : adapters_) {
        results.push_back(run_parallel_read(*adapter));
    }
    return results;
}

SliceReadResult BenchmarkRunner::run_slice_read(FormatAdapter& adapter) {
    SliceReadResult result;
    result.name = adapter.name();
    result.available = adapter.is_available();
    result.supports_slice = adapter.supports_slice_read();

    if (!result.available) {
        return result;
    }

    // Only makes sense for 3D volumes
    if (config_.ny <= 1) {
        result.error = "Slice read requires 3D volume (ny > 1)";
        return result;
    }

    ArrayShape shape{.nx=config_.nx, .nz=config_.nz, .ny=config_.ny};
    auto data = generate_data(shape);
    auto file_path = temp_dir_ / (adapter.name() + adapter.extension());
    std::string path_str = file_path.string();

    try {
        // Write the full volume
        adapter.write(path_str, data.data(), shape);

        // Get file size
        if (std::filesystem::is_directory(file_path)) {
            std::uintmax_t dir_size = 0;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(file_path)) {
                if (std::filesystem::is_regular_file(entry)) {
                    dir_size += std::filesystem::file_size(entry);
                }
            }
            result.file_size_mb = static_cast<double>(dir_size) / (1024.0 * 1024.0);
        } else {
            result.file_size_mb = static_cast<double>(std::filesystem::file_size(file_path)) / (1024.0 * 1024.0);
        }

        const std::size_t slice_elements = shape.nx * shape.nz;
        result.slice_size_mb = static_cast<double>(slice_elements * sizeof(float)) / (1024.0 * 1024.0);

        // Warmup
        {
            std::vector<float> warmup_full(shape.total());
            adapter.read(path_str, warmup_full.data(), shape);
            std::vector<float> warmup_slice(slice_elements);
            adapter.read_slice(path_str, warmup_slice.data(), shape, shape.ny / 2);
        }

        // Read the middle inline slice (iy = ny/2)
        std::vector<float> slice_buf(slice_elements);
        Timer slice_timer;
        slice_timer.start();
        adapter.read_slice(path_str, slice_buf.data(), shape, shape.ny / 2);
        slice_timer.stop();
        result.slice_read_ms = slice_timer.elapsed_ms();

        // Read the full volume for comparison
        std::vector<float> full_buf(shape.total());
        Timer full_timer;
        full_timer.start();
        adapter.read(path_str, full_buf.data(), shape);
        full_timer.stop();
        result.full_read_ms = full_timer.elapsed_ms();

        // Throughput
        result.slice_read_mbps = throughput_mbps(result.slice_size_mb, result.slice_read_ms / 1000.0);
        result.full_read_mbps = throughput_mbps(result.file_size_mb, result.full_read_ms / 1000.0);

        // Speedup: how much faster is slice read vs full read?
        // Ideal: ny (reading 1/ny of the data)
        if (result.slice_read_ms > 0.0) {
            result.speedup = result.full_read_ms / result.slice_read_ms;
        }

        // Verify slice data against source
        const float* expected = data.data() + (shape.ny / 2) * slice_elements;
        for (std::size_t i = 0; i < slice_elements; ++i) {
            if (std::abs(expected[i] - slice_buf[i]) > 0.001f) {
                result.error = "Slice data integrity check failed";
                break;
            }
        }

        // Cleanup
        std::filesystem::remove_all(file_path);

    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

std::vector<SliceReadResult> BenchmarkRunner::run_slice_read_all() {
    std::vector<SliceReadResult> results;
    results.reserve(adapters_.size());
    for (auto& adapter : adapters_) {
        results.push_back(run_slice_read(*adapter));
    }
    return results;
}

} // namespace io_bench
