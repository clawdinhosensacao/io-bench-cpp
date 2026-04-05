# I/O Format Benchmark (C++)

A comprehensive I/O format benchmark for scientific computing arrays, implemented entirely in C++20.

## Supported Formats

| Format | Status | Description |
|--------|--------|-------------|
| `binary_f32` | ✅ Always | Raw float32, no header |
| `binary_header` | ✅ Always | Float32 with shape header (magic + nx + nz) |
| `mmap` | ✅ Always | Memory-mapped binary (POSIX) |
| `npy` | ✅ Always | NumPy native format via cnpy |
| `json` | ✅ Always | JSON 2D array (nlohmann/json) |
| `hdf5` | Optional | HDF5 via HighFive |
| `netcdf` | Optional | NetCDF4 C++ |
| `tiledb` | Optional | TileDB dense array |
| `adios2` | Optional | ADIOS2 BP format |

## Quick Start

### Minimal Build (no optional deps)

```bash
make minimal
./build/io_bench --nx 100 --nz 80
```

### Full Build (with optional deps)

```bash
# Ubuntu
make install-deps-ubuntu
make build

# macOS
make install-deps-macos
make build
```

### Run Benchmark

```bash
# Quick test
make run

# Full benchmark
make bench

# Fast benchmark
make bench-fast
```

## Usage

```bash
./build/io_bench [options]

Options:
  --nx <n>           Grid size in X (default: 100)
  --nz <n>           Grid size in Z (default: 80)
  --iterations <n>   Number of iterations (default: 1)
  --output <path>    Output markdown report path
  --help             Show help
```

### Examples

```bash
# Small test
./build/io_bench --nx 50 --nz 40

# Production benchmark
./build/io_bench --nx 400 --nz 300 --iterations 5 --output results.md

# Compare with Python version
./build/io_bench --nx 80 --nz 100 --iterations 3 --output cpp_results.md
cd ../rtm3d-cli && python3 scripts/io_format_benchmark.py --nx 80 --nz 100 --iterations 3 --report python_results.md
```

## Building with CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Optional Dependencies

To enable HDF5, NetCDF, TileDB, or ADIOS2 support:

```bash
# Ubuntu
apt install libhdf5-dev libnetcdf-c++4-dev libtiledb-dev libadios2-dev

# CMake will auto-detect
cmake -S . -B build
```

## Output Format

The benchmark outputs a markdown table:

```
| Format | Status | Size (MB) | Write (ms) | Read (ms) | Write MB/s | Read MB/s | Notes |
|--------|--------|-----------|------------|-----------|------------|-----------|-------|
| binary_f32 | ✅ | 0.031 | 0.05 | 0.03 | 620 | 1033 | - |
| npy | ✅ | 0.032 | 0.12 | 0.08 | 258 | 387 | - |
| json | ✅ | 0.158 | 1.45 | 2.12 | 109 | 74 | - |
...
```

## API Usage

```cpp
#include "io_bench/benchmark.hpp"
#include "io_bench/formats.hpp"

int main() {
    io_bench::BenchConfig config;
    config.nx = 100;
    config.nz = 80;
    config.iterations = 3;
    
    io_bench::BenchmarkRunner runner(config);
    
    // Add specific formats
    runner.add_format(std::make_unique<io_bench::BinaryFormat>());
    runner.add_format(std::make_unique<io_bench::NpyFormat>());
    
    auto results = runner.run_all();
    
    for (const auto& r : results) {
        std::cout << r.name << ": " << r.read_mbps << " MB/s read\n";
    }
    
    return 0;
}
```

## Comparison with Python

This C++ implementation mirrors the Python benchmark in `rtm3d-cli/scripts/io_format_benchmark.py`:

| Aspect | Python | C++ |
|--------|--------|-----|
| JSON | nlohmann/json | json module |
| Binary | std::ofstream | numpy.tofile |
| NPY | cnpy | numpy |
| HDF5 | HighFive | h5py |
| Memory map | POSIX mmap | numpy.memmap |

Expected performance difference: C++ should show 10-50% higher throughput due to:
- No interpreter overhead
- Direct filesystem I/O
- Compile-time optimizations

## Test

```bash
make test
```

Runs unit tests for:
- Data generation
- Timer accuracy
- Format adapters
- Round-trip integrity

## License

MIT
