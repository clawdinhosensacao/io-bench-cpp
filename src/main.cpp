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

static const std::vector<GeoPreset> geo_presets = {
    {"2d-survey-line",
     "2D seismic survey line (typical 2D marine: 480 traces × 1501 samples)",
     480, 1501, 1, 3},
    {"2d-velocity-model",
     "2D velocity model for RTM (typical: 401 × 201 grid, 10m spacing)",
     401, 201, 1, 3},
    {"3d-velocity-model",
     "3D velocity model for RTM (typical: 401 × 201 × 201, ~60 MB float32)",
     401, 201, 201, 2},
    {"3d-large-survey",
     "Large 3D survey volume (600 × 400 × 300, ~275 MB float32)",
     600, 400, 300, 1},
    {"3d-big-volume",
     "Big 3D volume for throughput at realistic seismic scale (401 × 401 × 501, ~307 MB)",
     401, 401, 501, 1},
    {"shot-gather",
     "Single shot gather (640 receivers × 4001 time samples, ~10 MB)",
     640, 4001, 1, 5},
    {"checkpoint-restart",
     "RTM checkpoint/restart volume (200 × 100 × 100, ~8 MB, many iterations)",
     200, 100, 100, 10},
};

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --nx <n>           Grid size in X (default: 100)\n"
              << "  --nz <n>           Grid size in Z (default: 80)\n"
              << "  --ny <n>           Grid size in Y for 3D (default: 1)\n"
              << "  --iterations <n>   Number of iterations (default: 1)\n"
              << "  --threads <n>      Number of parallel read threads (default: 1, sequential)\n"
              << "  --slice-read       Run inline slice read benchmark (requires 3D volume, ny>1)\n"
              << "  --big-volume       Shortcut for --preset 3d-big-volume (401×401×501, ~307 MB)\n"
              << "  --preset <name>    Use geophysics preset (overrides nx/nz/ny/iterations)\n"
              << "  --list-presets     List available geophysics presets\n"
              << "  --output <path>    Output markdown report path\n"
              << "  --help             Show this message\n\n"
              << "Geophysics Presets:\n";
    for (const auto& p : geo_presets) {
        const double mb = static_cast<double>(p.nx * p.nz * p.ny) * sizeof(float) / (1024.0 * 1024.0);
        std::cout << "  " << p.name << "\n"
                  << "    " << p.description << "\n"
                  << "    Grid: " << p.nx << " × " << p.nz << " × " << p.ny
                  << " (" << mb << " MB, " << p.iterations << " iter)\n\n";
    }
}

void list_presets() {
    std::cout << "Available geophysics presets:\n\n";
    for (const auto& p : geo_presets) {
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
        for (const auto& p : geo_presets) {
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
    
    // Write report if requested
    if (!output_path.empty()) {
        auto report = io_bench::generate_markdown_report(results, config);
        io_bench::write_report(output_path, report);
        std::cout << "\nReport written to: " << output_path << "\n";
    }
    
    return 0;
}
