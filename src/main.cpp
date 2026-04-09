#include "io_bench/benchmark.hpp"
#include "io_bench/report.hpp"
#include "io_bench/formats.hpp"

#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>

/// Geophysics-relevant benchmark presets
struct GeoPreset {
    std::string name;
    std::string description;
    std::size_t nx, nz, ny;
    std::size_t iterations;
};

static const std::vector<GeoPreset>& get_geo_presets() {
    static const std::vector<GeoPreset> geo_presets = {
    {.name = "2d-survey-line",
     .description = "2D seismic survey line (typical 2D marine: 480 traces × 1501 samples)",
     .nx = 480, .nz = 1501, .ny = 1, .iterations = 3},
    {.name = "2d-velocity-model",
     .description = "2D velocity model for RTM (typical: 401 × 201 grid, 10m spacing)",
     .nx = 401, .nz = 201, .ny = 1, .iterations = 3},
    {.name = "3d-velocity-model",
     .description = "3D velocity model for RTM (typical: 401 × 201 × 201, ~60 MB float32)",
     .nx = 401, .nz = 201, .ny = 201, .iterations = 2},
    {.name = "3d-large-survey",
     .description = "Large 3D survey volume (600 × 400 × 300, ~275 MB float32)",
     .nx = 600, .nz = 400, .ny = 300, .iterations = 1},
    {.name = "3d-big-volume",
     .description = "Big 3D volume for throughput at realistic seismic scale (401 × 401 × 501, ~307 MB)",
     .nx = 401, .nz = 401, .ny = 501, .iterations = 1},
    {.name = "shot-gather",
     .description = "Single shot gather (640 receivers × 4001 time samples, ~10 MB)",
     .nx = 640, .nz = 4001, .ny = 1, .iterations = 5},
    {.name = "checkpoint-restart",
     .description = "RTM checkpoint/restart volume (200 × 100 × 100, ~8 MB, many iterations)",
     .nx = 200, .nz = 100, .ny = 100, .iterations = 10},
    };
    return geo_presets;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --nx <n>           Grid size in X (default: 100)\n"
              << "  --nz <n>           Grid size in Z (default: 80)\n"
              << "  --ny <n>           Grid size in Y for 3D (default: 1)\n"
              << "  --iterations <n>   Number of iterations (default: 1)\n"
              << "  --threads <n>      Number of parallel read threads (default: 1, sequential)\n"
              << "  --slice-read       Run inline slice read benchmark (requires 3D volume, ny>1)\n"
              << "  --trace-read       Run trace read benchmark (sequential + random trace access)\n"
              << "  --stream-write     Run streaming write benchmark (append-only trace writes)\n"
              << "  --checkpoint       Run checkpoint/restart benchmark (write-then-read with integrity check)\n"
              << "  --big-volume       Shortcut for --preset 3d-big-volume (401×401×501, ~307 MB)\n"
              << "  --preset <name>    Use geophysics preset (overrides nx/nz/ny/iterations)\n"
              << "  --list-presets     List available geophysics presets\n"
              << "  --output <path>    Output markdown report path\n"
              << "  --help             Show this message\n\n"
              << "Geophysics Presets:\n";
    for (const auto& p : get_geo_presets()) {
        const double mb = static_cast<double>(p.nx * p.nz * p.ny) * sizeof(float) / (1024.0 * 1024.0);
        std::cout << "  " << p.name << "\n"
                  << "    " << p.description << "\n"
                  << "    Grid: " << p.nx << " × " << p.nz << " × " << p.ny
                  << " (" << mb << " MB, " << p.iterations << " iter)\n\n";
    }
}

void list_presets() {
    std::cout << "Available geophysics presets:\n\n";
    for (const auto& p : get_geo_presets()) {
        const double mb = static_cast<double>(p.nx * p.nz * p.ny) * sizeof(float) / (1024.0 * 1024.0);
        std::cout << "  " << p.name << "\n"
                  << "    " << p.description << "\n"
                  << "    Grid: " << p.nx << " × " << p.nz << " × " << p.ny
                  << " (" << mb << " MB, " << p.iterations << " iter)\n\n";
    }
}

int main(int argc, char* argv[]) {
    io_bench::BenchConfig config;
    std::string output_path;
    std::string preset_name;
    bool list_only = false;
    bool slice_read = false;
    bool trace_read = false;
    bool stream_write = false;
    bool checkpoint = false;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        
        auto get_value = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Error: missing value for " << arg << "\n";
                std::exit(1);
            }
            return argv[++i];
        };
        
        if (arg == "--nx") {
            config.nx = std::stoul(get_value());
        } else if (arg == "--nz") {
            config.nz = std::stoul(get_value());
        } else if (arg == "--ny") {
            config.ny = std::stoul(get_value());
        } else if (arg == "--iterations") {
            config.iterations = std::stoul(get_value());
        } else if (arg == "--threads") {
            config.parallel_threads = std::stoul(get_value());
        } else if (arg == "--slice-read") {
            slice_read = true;
        } else if (arg == "--trace-read") {
            trace_read = true;
        } else if (arg == "--stream-write") {
            stream_write = true;
        } else if (arg == "--checkpoint") {
            checkpoint = true;
        } else if (arg == "--big-volume") {
            preset_name = "3d-big-volume";
        } else if (arg == "--preset") {
            preset_name = get_value();
        } else if (arg == "--list-presets") {
            list_only = true;
        } else if (arg == "--output") {
            output_path = get_value();
        } else {
            std::cerr << "Error: unknown option " << arg << "\n";
            return 1;
        }
    }

    if (list_only) {
        list_presets();
        return 0;
    }

    // Apply preset if specified
    if (!preset_name.empty()) {
        bool found = false;
        for (const auto& p : get_geo_presets()) {
            if (p.name == preset_name) {
                config.nx = p.nx;
                config.nz = p.nz;
                config.ny = p.ny;
                config.iterations = p.iterations;
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "Error: unknown preset '" << preset_name << "'\n";
            std::cerr << "Use --list-presets to see available presets.\n";
            return 1;
        }
    }
    
    std::cout << "I/O Format Benchmark (C++)\n";
    std::cout << "===========================\n";
    if (config.ny > 1) {
        std::cout << "Grid: " << config.nx << " x " << config.nz << " x " << config.ny << " (3D)\n";
    } else {
        std::cout << "Grid: " << config.nx << " x " << config.nz << "\n";
    }
    if (!preset_name.empty()) {
        std::cout << "Preset: " << preset_name << "\n";
    }
    std::cout << "Iterations: " << config.iterations << "\n\n";
    
    // Create runner and add formats
    io_bench::BenchmarkRunner runner(config);
    auto adapters = io_bench::create_all_adapters();
    for (auto& adapter : adapters) {
        runner.add_format(std::move(adapter));
    }
    
    // Run benchmarks
    std::cout << "Running benchmarks...\n";
    auto results = runner.run_all();
    
    // Print results
    io_bench::print_results(results);
    
    // Parallel read benchmark if threads > 1
    if (config.parallel_threads > 1) {
        std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "Parallel Read Benchmark (" << config.parallel_threads << " threads)\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
        
        auto par_results = runner.run_parallel_read_all();
        
        // Print header
        std::cout << std::left << std::setw(18) << "Format"
                  << std::right << std::setw(8) << "Avail"
                  << std::setw(10) << "Total ms"
                  << std::setw(12) << "Per-Th ms"
                  << std::setw(12) << "Data MB"
                  << std::setw(14) << "Agg MB/s"
                  << "\n";
        std::cout << std::string(74, '-') << "\n";
        
        for (const auto& r : par_results) {
            if (!r.available) {
                std::cout << std::left << std::setw(18) << r.name
                          << std::right << "  N/A\n";
                continue;
            }
            if (!r.error.empty()) {
                std::cout << std::left << std::setw(18) << r.name
                          << "  ERROR: " << r.error << "\n";
                continue;
            }
            std::cout << std::left << std::setw(18) << r.name
                      << std::right << std::setw(8) << "OK"
                      << std::setw(10) << std::fixed << std::setprecision(1) << r.total_read_ms
                      << std::setw(12) << std::setprecision(1) << r.per_thread_ms
                      << std::setw(12) << std::setprecision(1) << r.data_size_mb
                      << std::setw(14) << std::setprecision(1) << r.aggregate_mbps
                      << "\n";
        }
    }
    
    // Slice read benchmark
    if (slice_read) {
        if (config.ny <= 1) {
            std::cerr << "\nError: --slice-read requires a 3D volume. Use --ny N (N>1) or a 3D preset.\n";
        } else {
            std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            std::cout << "Inline Slice Read Benchmark (3D: " << config.nx << "×" << config.nz << "×" << config.ny << ")\n";
            std::cout << "Reading middle inline (iy=" << config.ny/2 << ") vs full volume\n";
            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
            
            auto slice_results = runner.run_slice_read_all();
            
            std::cout << std::left << std::setw(18) << "Format"
                      << std::right << std::setw(6) << "Slice"
                      << std::setw(12) << "Slice MB"
                      << std::setw(12) << "Slice ms"
                      << std::setw(12) << "Slice MB/s"
                      << std::setw(12) << "Full ms"
                      << std::setw(12) << "Full MB/s"
                      << std::setw(10) << "Speedup"
                      << "\n";
            std::cout << std::string(92, '-') << "\n";
            
            for (const auto& r : slice_results) {
                if (!r.available) {
                    std::cout << std::left << std::setw(18) << r.name
                              << std::right << "  N/A\n";
                    continue;
                }
                if (!r.error.empty()) {
                    std::cout << std::left << std::setw(18) << r.name
                              << "  ERROR: " << r.error << "\n";
                    continue;
                }
                std::cout << std::left << std::setw(18) << r.name
                          << std::right << std::setw(6) << (r.supports_slice ? "✓" : "✗")
                          << std::setw(12) << std::fixed << std::setprecision(2) << r.slice_size_mb
                          << std::setw(12) << std::setprecision(1) << r.slice_read_ms
                          << std::setw(12) << std::setprecision(1) << r.slice_read_mbps
                          << std::setw(12) << std::setprecision(1) << r.full_read_ms
                          << std::setw(12) << std::setprecision(1) << r.full_read_mbps
                          << std::setw(10) << std::setprecision(1) << r.speedup << "x"
                          << "\n";
            }
            
            std::cout << "\n  ✓ = native slice support  ✗ = fallback (read all, extract slice)\n";
            std::cout << "  Ideal speedup = " << config.ny << "x (reading 1/" << config.ny << " of volume)\n";
        }
    }

    // Trace read benchmark
    if (trace_read) {
        std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "Trace Read Benchmark (" << config.nx * config.ny << " traces, "
                  << config.nz << " samples/trace, "
                  << std::fixed << std::setprecision(1)
                  << (static_cast<double>(config.nz * sizeof(float)) / 1024.0) << " KB/trace)\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";

        auto trace_results = runner.run_trace_read_all();

        std::cout << std::left << std::setw(18) << "Format"
                  << std::right << std::setw(8) << "Traces"
                  << std::setw(12) << "Seq ms"
                  << std::setw(12) << "Seq MB/s"
                  << std::setw(12) << "Rand ms"
                  << std::setw(12) << "Rand MB/s"
                  << "\n";
        std::cout << std::string(74, '-') << "\n";

        for (const auto& r : trace_results) {
            if (!r.available) {
                std::cout << std::left << std::setw(18) << r.name
                          << std::right << "  N/A\n";
                continue;
            }
            if (!r.error.empty()) {
                std::cout << std::left << std::setw(18) << r.name
                          << "  ERROR: " << r.error << "\n";
                continue;
            }
            std::cout << std::left << std::setw(18) << r.name
                      << std::right << std::setw(8) << r.num_traces
                      << std::setw(12) << std::fixed << std::setprecision(1) << r.sequential_read_ms
                      << std::setw(12) << std::setprecision(1) << r.sequential_mbps
                      << std::setw(12) << std::setprecision(1) << r.random_read_ms
                      << std::setw(12) << std::setprecision(1) << r.random_mbps
                      << "\n";
        }
    }

    // Streaming write benchmark
    if (stream_write) {
        std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "Streaming Write Benchmark (" << config.nx * config.ny << " traces, "
                  << config.nz << " samples/trace, "
                  << std::fixed << std::setprecision(1)
                  << (static_cast<double>(config.nz * sizeof(float)) / 1024.0) << " KB/trace)\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";

        auto stream_results = runner.run_stream_write_all();

        std::cout << std::left << std::setw(18) << "Format"
                  << std::right << std::setw(12) << "Stream ms"
                  << std::setw(12) << "Stream MB/s"
                  << std::setw(10) << "Bulk ms"
                  << std::setw(12) << "Bulk MB/s"
                  << std::setw(10) << "Slowdown"
                  << "\n";
        std::cout << std::string(74, '-') << "\n";

        for (const auto& r : stream_results) {
            if (!r.available) {
                std::cout << std::left << std::setw(18) << r.name
                          << std::right << "  N/A\n";
                continue;
            }
            if (!r.error.empty()) {
                std::cout << std::left << std::setw(18) << r.name
                          << "  ERROR: " << r.error << "\n";
                continue;
            }
            std::cout << std::left << std::setw(18) << r.name
                      << std::right << std::setw(12) << std::fixed << std::setprecision(1) << r.stream_write_ms
                      << std::setw(12) << std::setprecision(1) << r.stream_write_mbps
                      << std::setw(10) << std::setprecision(1) << r.bulk_write_ms
                      << std::setw(12) << std::setprecision(1) << r.bulk_write_mbps
                      << std::setw(10) << std::setprecision(1) << r.slowdown << "x"
                      << "\n";
        }

        std::cout << "\n  Slowdown = stream_write_ms / bulk_write_ms (1.0x = no penalty)\n";
    }

    // Checkpoint/restart benchmark
    if (checkpoint) {
        std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "Checkpoint/Restart Benchmark (" << config.nx << "×" << config.nz;
        if (config.ny > 1) { std::cout << "×" << config.ny; }
        std::cout << ", " << std::fixed << std::setprecision(1)
                  << io_bench::ArrayShape{config.nx, config.nz, config.ny}.mb() << " MB)\n";
        std::cout << "Write → Read-back with integrity verification\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";

        auto ckpt_results = runner.run_checkpoint_all();

        std::cout << std::left << std::setw(18) << "Format"
                  << std::right << std::setw(8) << "Write ms"
                  << std::setw(8) << "Read ms"
                  << std::setw(10) << "Total ms"
                  << std::setw(10) << "W MB/s"
                  << std::setw(10) << "R MB/s"
                  << std::setw(8) << "OK?"
                  << "\n";
        std::cout << std::string(72, '-') << "\n";

        for (const auto& r : ckpt_results) {
            if (!r.available) {
                std::cout << std::left << std::setw(18) << r.name
                          << std::right << "  N/A\n";
                continue;
            }
            if (!r.error.empty()) {
                std::cout << std::left << std::setw(18) << r.name
                          << "  ERROR: " << r.error << "\n";
                continue;
            }
            std::cout << std::left << std::setw(18) << r.name
                      << std::right << std::setw(8) << std::fixed << std::setprecision(1) << r.write_ms
                      << std::setw(8) << std::setprecision(1) << r.read_ms
                      << std::setw(10) << std::setprecision(1) << r.round_trip_ms
                      << std::setw(10) << std::setprecision(1) << r.write_mbps
                      << std::setw(10) << std::setprecision(1) << r.read_mbps
                      << std::setw(8) << (r.integrity_ok ? "✓" : "✗")
                      << "\n";
        }

        std::cout << "\n  ✓ = data integrity verified (read-back matches written data)\n";
    }

    // Write report if requested
    if (!output_path.empty()) {
        auto report = io_bench::generate_markdown_report(results, config);
        io_bench::write_report(output_path, report);
        std::cout << "\nReport written to: " << output_path << "\n";
    }
    
    return 0;
}
