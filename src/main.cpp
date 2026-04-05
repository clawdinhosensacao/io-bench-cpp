#include "io_bench/benchmark.hpp"
#include "io_bench/report.hpp"
#include "io_bench/formats.hpp"

#include <iostream>
#include <string>
#include <cstdlib>

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --nx <n>           Grid size in X (default: 100)\n"
              << "  --nz <n>           Grid size in Z (default: 80)\n"
              << "  --iterations <n>   Number of iterations (default: 1)\n"
              << "  --output <path>    Output markdown report path\n"
              << "  --help             Show this message\n";
}

int main(int argc, char* argv[]) {
    io_bench::BenchConfig config;
    std::string output_path;
    
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
        } else if (arg == "--iterations") {
            config.iterations = std::stoul(get_value());
        } else if (arg == "--output") {
            output_path = get_value();
        } else {
            std::cerr << "Error: unknown option " << arg << "\n";
            return 1;
        }
    }
    
    std::cout << "I/O Format Benchmark (C++)\n";
    std::cout << "===========================\n";
    std::cout << "Grid: " << config.nx << " x " << config.nz << "\n";
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
    
    // Write report if requested
    if (!output_path.empty()) {
        auto report = io_bench::generate_markdown_report(results, config);
        io_bench::write_report(output_path, report);
        std::cout << "\nReport written to: " << output_path << "\n";
    }
    
    return 0;
}
