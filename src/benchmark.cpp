#include "io_bench/benchmark.hpp"
#include "io_bench/formats.hpp"
#include <random>
#include <filesystem>
#include <utility>
#include <thread>
#include <atomic>

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
    } catch (...) {  // NOLINT(bugprone-empty-catch)
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
            result.peak_write_rss_mb = std::max(result.peak_write_rss_mb, get_rss_mb());
            
            // Get file size after write
            result.file_size_mb = file_size_mb(file_path);
            
            // Read
            Timer read_timer;
            read_timer.start();
            adapter.read(path_str, read_buffer.data(), shape);
            read_timer.stop();
            total_read_ms += read_timer.elapsed_ms();
            result.peak_read_rss_mb = std::max(result.peak_read_rss_mb, get_rss_mb());
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
    adapters.push_back(std::make_unique<DirectIOFormat>());
    
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
        const double fsize_mb = file_size_mb(file_path);

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
            threads.emplace_back([&, t]() {  // NOLINT(performance-inefficient-vector-operation)
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
        result.data_size_mb = fsize_mb * static_cast<double>(num_threads);
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
        result.file_size_mb = file_size_mb(file_path);

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
        const float* expected = data.data() + ((shape.ny / 2) * slice_elements);
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

TraceReadResult BenchmarkRunner::run_trace_read(FormatAdapter& adapter) {
    TraceReadResult result;
    result.name = adapter.name();

    if (!adapter.is_available()) {
        result.available = false;
        return result;
    }
    result.available = true;

    const ArrayShape shape{config_.nx, config_.nz, config_.ny};
    const std::size_t num_traces = shape.nx * shape.ny;
    const std::size_t samples_per_trace = shape.nz;
    const double trace_size_kb = static_cast<double>(samples_per_trace * sizeof(float)) / 1024.0;

    result.num_traces = num_traces;
    result.samples_per_trace = samples_per_trace;
    result.trace_size_kb = trace_size_kb;

    try {
        // Generate data and write file
        auto data = generate_data(shape);
        auto file_path = temp_dir_ / (adapter.name() + adapter.extension());

        adapter.write(file_path.string(), data.data(), shape);

        // File size
        result.file_size_mb = file_size_mb(file_path);

        // --- Sequential trace read: read all traces one by one ---
        {
            std::vector<float> trace_buf(samples_per_trace);
            Timer timer;
            timer.start();
            for (std::size_t t = 0; t < num_traces; ++t) {
                adapter.read_trace(file_path.string(), trace_buf.data(), shape, t);
            }
            timer.stop();
            result.sequential_read_ms = timer.elapsed_ms();
            const double data_mb = static_cast<double>(num_traces * samples_per_trace * sizeof(float)) / (1024.0 * 1024.0);
            result.sequential_mbps = throughput_mbps(data_mb, timer.elapsed_s());
        }

        // --- Random trace read: read 100 random traces ---
        {
            const std::size_t random_count = std::min(num_traces, std::size_t{100});
            std::vector<std::size_t> trace_indices(random_count);
            // Pseudo-random trace selection (deterministic seed)
            for (std::size_t i = 0; i < random_count; ++i) {
                trace_indices[i] = (i * 7919 + 1) % num_traces;
            }
            std::vector<float> trace_buf(samples_per_trace);
            Timer timer;
            timer.start();
            for (std::size_t i = 0; i < random_count; ++i) {
                adapter.read_trace(file_path.string(), trace_buf.data(), shape, trace_indices[i]);
            }
            timer.stop();
            result.random_read_ms = timer.elapsed_ms();
            result.random_traces_read = static_cast<double>(random_count);
            const double data_mb = static_cast<double>(random_count * samples_per_trace * sizeof(float)) / (1024.0 * 1024.0);
            result.random_mbps = throughput_mbps(data_mb, timer.elapsed_s());
        }

        // Cleanup
        std::filesystem::remove_all(file_path);

    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

std::vector<TraceReadResult> BenchmarkRunner::run_trace_read_all() {
    std::vector<TraceReadResult> results;
    results.reserve(adapters_.size());
    for (auto& adapter : adapters_) {
        results.push_back(run_trace_read(*adapter));
    }
    return results;
}

StreamWriteResult BenchmarkRunner::run_stream_write(FormatAdapter& adapter) {
    StreamWriteResult result;
    result.name = adapter.name();

    if (!adapter.is_available()) {
        result.available = false;
        return result;
    }
    result.available = true;

    const ArrayShape shape{config_.nx, config_.nz, config_.ny};
    const std::size_t num_traces = shape.nx * shape.ny;
    const std::size_t samples_per_trace = shape.nz;
    const double trace_size_kb = static_cast<double>(samples_per_trace * sizeof(float)) / 1024.0;

    result.num_traces = num_traces;
    result.samples_per_trace = samples_per_trace;
    result.trace_size_kb = trace_size_kb;

    try {
        // Generate data
        auto data = generate_data(shape);

        // --- Bulk write (baseline) ---
        {
            auto file_path = temp_dir_ / (adapter.name() + "_bulk" + adapter.extension());
            Timer timer;
            timer.start();
            adapter.write(file_path.string(), data.data(), shape);
            timer.stop();
            result.bulk_write_ms = timer.elapsed_ms();
            const double data_mb = shape.mb();
            result.bulk_write_mbps = throughput_mbps(data_mb, timer.elapsed_s());
            std::filesystem::remove_all(file_path);
        }

        // --- Streaming write: trace by trace ---
        if (adapter.supports_stream_write()) {
            auto file_path = temp_dir_ / (adapter.name() + "_stream" + adapter.extension());
            Timer timer;
            timer.start();
            for (std::size_t t = 0; t < num_traces; ++t) {
                const float* trace_data = data.data() + (t * samples_per_trace);
                adapter.write_trace(file_path.string(), trace_data, shape, t);
            }
            timer.stop();
            result.stream_write_ms = timer.elapsed_ms();
            const double data_mb = shape.mb();
            result.stream_write_mbps = throughput_mbps(data_mb, timer.elapsed_s());

            if (result.bulk_write_ms > 0.0) {
                result.slowdown = result.stream_write_ms / result.bulk_write_ms;
            }

            std::filesystem::remove_all(file_path);
        } else {
            // Format does not support streaming writes
            result.stream_write_ms = 0.0;
            result.stream_write_mbps = 0.0;
            result.slowdown = 0.0;
        }

    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

std::vector<StreamWriteResult> BenchmarkRunner::run_stream_write_all() {
    std::vector<StreamWriteResult> results;
    results.reserve(adapters_.size());
    for (auto& adapter : adapters_) {
        results.push_back(run_stream_write(*adapter));
    }
    return results;
}

CheckpointResult BenchmarkRunner::run_checkpoint(FormatAdapter& adapter) {
    CheckpointResult result;
    result.name = adapter.name();

    if (!adapter.is_available()) {
        result.available = false;
        return result;
    }
    result.available = true;

    const ArrayShape shape{config_.nx, config_.nz, config_.ny};

    try {
        // Generate data
        auto data = generate_data(shape);
        auto file_path = temp_dir_ / (adapter.name() + adapter.extension());

        // --- Checkpoint: write ---
        Timer write_timer;
        write_timer.start();
        adapter.write(file_path.string(), data.data(), shape);
        write_timer.stop();
        result.write_ms = write_timer.elapsed_ms();
        result.write_mbps = throughput_mbps(shape.mb(), write_timer.elapsed_s());

        // File size
        result.file_size_mb = file_size_mb(file_path);

        // --- Restart: read back ---
        std::vector<float> read_data(shape.total());
        Timer read_timer;
        read_timer.start();
        adapter.read(file_path.string(), read_data.data(), shape);
        read_timer.stop();
        result.read_ms = read_timer.elapsed_ms();
        result.read_mbps = throughput_mbps(shape.mb(), read_timer.elapsed_s());

        // Round-trip time
        result.round_trip_ms = result.write_ms + result.read_ms;

        // --- Integrity check ---
        double max_err = 0.0;
        bool ok = true;
        for (std::size_t i = 0; i < shape.total(); ++i) {
            double err = std::abs(static_cast<double>(data[i]) - static_cast<double>(read_data[i]));
            if (err > max_err) { max_err = err; }
            // Allow small floating-point differences from format conversions
            if (err > 1e-3) { ok = false; }
        }
        result.integrity_ok = ok;
        result.max_abs_error = max_err;

        // Cleanup
        std::filesystem::remove_all(file_path);

    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

std::vector<CheckpointResult> BenchmarkRunner::run_checkpoint_all() {
    std::vector<CheckpointResult> results;
    results.reserve(adapters_.size());
    for (auto& adapter : adapters_) {
        results.push_back(run_checkpoint(*adapter));
    }
    return results;
}

CompressionSweepResult BenchmarkRunner::run_compression_sweep(const std::string& format_name) {
    CompressionSweepResult result;
    result.name = format_name;

    // Find the adapter
    FormatAdapter* adapter = nullptr;
    for (auto& a : adapters_) {
        if (a->name() == format_name) {
            adapter = a.get();
            break;
        }
    }

    if (!adapter) {
        result.error = "Format not found: " + format_name;
        return result;
    }

    if (!adapter->is_available()) {
        result.available = false;
        return result;
    }
    result.available = true;

    if (!adapter->supports_compression_sweep()) {
        result.error = "Format does not support compression sweep";
        return result;
    }

    result.compressor = adapter->compressor_name();
    const ArrayShape shape{config_.nx, config_.nz, config_.ny};
    result.raw_data_mb = shape.mb();

    // Generate data once
    auto data = generate_data(shape);

    // Test compression levels 0-9
    for (int level = 0; level <= 9; ++level) {
        CompressionLevelResult lvl;
        lvl.level = level;

        try {
            auto file_path = temp_dir_ / (adapter->name() + "_c" + std::to_string(level) + adapter->extension());

            // Write with compression level
            Timer write_timer;
            write_timer.start();
            adapter->write_compressed(file_path.string(), data.data(), shape, level);
            write_timer.stop();
            lvl.write_ms = write_timer.elapsed_ms();
            lvl.write_mbps = throughput_mbps(shape.mb(), write_timer.elapsed_s());

            // File size
            lvl.file_size_mb = file_size_mb(file_path);
            lvl.compression_ratio = (lvl.file_size_mb > 0.0) ? shape.mb() / lvl.file_size_mb : 0.0;

            // Read back
            std::vector<float> read_data(shape.total());
            Timer read_timer;
            read_timer.start();
            adapter->read(file_path.string(), read_data.data(), shape);
            read_timer.stop();
            lvl.read_ms = read_timer.elapsed_ms();
            lvl.read_mbps = throughput_mbps(shape.mb(), read_timer.elapsed_s());

            // Cleanup
            std::filesystem::remove_all(file_path);

        } catch (const std::exception& e) {
            result.error = "Level " + std::to_string(level) + ": " + e.what();
            break;
        }

        result.levels.push_back(lvl);
    }

    return result;
}

std::vector<CompressionSweepResult> BenchmarkRunner::run_compression_sweep_all() {
    std::vector<CompressionSweepResult> results;
    for (auto& adapter : adapters_) {
        if (adapter->supports_compression_sweep() && adapter->is_available()) {
            results.push_back(run_compression_sweep(adapter->name()));
        }
    }
    return results;
}

} // namespace io_bench
