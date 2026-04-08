# I/O Format Benchmark (C++)

A comprehensive I/O format benchmark for scientific computing arrays, implemented entirely in C++20. Focused on **geophysics and seismic processing** workloads.

## Supported Formats (15 formats, 14 working)

| Format | Status | Description |
|--------|--------|-------------|
| `binary_f32` | ✅ Always | Raw float32, no header |
| `binary_header` | ✅ Always | Float32 with shape header (magic + nx + nz) |
| `mmap` | ✅ Always | Memory-mapped binary (POSIX) |
| `npy` | ✅ Always | NumPy native format via cnpy |
| `json` | ✅ Always | JSON 2D array (nlohmann/json) |
| `hdf5` | ✅ Optional | HDF5 via HighFive |
| `netcdf` | ✅ Optional | NetCDF4 C++ |
| `tiledb` | ✅ Optional | TileDB dense array |
| `zarr` | ✅ Optional | Zarr v2 via Python subprocess |
| `segy` | ✅ Optional | SEG-Y seismic trace format via segyio |
| `duckdb` | ✅ Optional | DuckDB columnar SQL engine |
| `parquet` | ✅ Optional | Apache Parquet via Arrow C++ |
| `tensorstore` | ✅ Optional | TensorStore via Python bridge |
| `mdio` | ✅ Optional | MDIO (Multidimensional IO) via Python bridge |
| `miniseed` | ✅ Optional | MiniSEED seismological time series via obspy |
| `adios2` | ❌ N/A | ADIOS2 BP format (no library available) |

## Geophysics Presets

Built-in benchmark scenarios for typical geophysics workloads:

| Preset | Grid | Size | Description |
|--------|------|------|-------------|
| `2d-survey-line` | 480 × 1501 | 2.7 MB | 2D marine seismic survey line |
| `2d-velocity-model` | 401 × 201 | 0.3 MB | 2D velocity model for RTM |
| `3d-velocity-model` | 401 × 201 × 201 | 62 MB | 3D velocity model for RTM |
| `3d-large-survey` | 600 × 400 × 300 | 275 MB | Large 3D survey volume |
| `shot-gather` | 640 × 4001 | 9.8 MB | Single shot gather (traces × time) |
| `checkpoint-restart` | 200 × 100 × 100 | 7.6 MB | RTM checkpoint/restart volume |

```bash
# List available presets
./build/io_bench --list-presets

# Run a geophysics preset
./build/io_bench --preset 3d-velocity-model --output artifacts/3d_velocity.md
./build/io_bench --preset shot-gather --output artifacts/shot_gather.md
```

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

# Geophysics preset
./build/io_bench --preset 2d-velocity-model
```

## Usage

```bash
./build/io_bench [options]

Options:
  --nx <n>              Grid size in X (default: 100)
  --nz <n>              Grid size in Z (default: 80)
  --ny <n>              Grid size in Y for 3D (default: 1)
  --iterations <n>      Number of iterations (default: 1)
  --preset <name>       Use geophysics preset (overrides nx/nz/ny/iterations)
  --list-presets        List available geophysics presets
  --output <path>       Output markdown report path
  --help                Show help
```

### Examples

```bash
# Small test
./build/io_bench --nx 50 --nz 40

# 3D benchmark
./build/io_bench --nx 100 --nz 80 --ny 50 --iterations 3

# Geophysics presets
./build/io_bench --preset 2d-survey-line
./build/io_bench --preset 3d-velocity-model
./build/io_bench --preset shot-gather

# Production benchmark with report
./build/io_bench --preset 3d-velocity-model --output results.md
```

## Building with CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Optional Dependencies

To enable HDF5, NetCDF, TileDB, SEG-Y, Parquet, DuckDB, or Zarr support:

```bash
# Ubuntu
apt install libhdf5-dev libnetcdf-c++4-dev libtiledb-dev
pip install segyio zarr tensorstore

# CMake / Make will auto-detect available libraries
```

## Output Format

The benchmark outputs a markdown table:

```
| Format | Status | Size (MB) | Write (ms) | Read (ms) | Write MB/s | Read MB/s |
|--------|--------|-----------|------------|-----------|------------|----------|
| binary_f32 | OK | 0.031 | 0.05 | 0.03 | 620 | 1033 |
| npy | OK | 0.032 | 0.12 | 0.08 | 258 | 387 |
| hdf5 | OK | 0.032 | 0.38 | 0.14 | 85 | 233 |
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
