# I/O Format Benchmark (C++)

A comprehensive I/O format benchmark for scientific computing arrays, implemented entirely in C++20. Focused on **geophysics and seismic processing** workloads with **direct access**, **parallel I/O**, and **chunked slice reads**.

## Supported Formats (20 formats)

| Format | Status | Type | Slice | Thread-safe | Trace | Stream | Description |
|--------|--------|------|-------|-------------|-------|--------|-------------|
| `binary_f32` | ✅ Always | Native C++ | ✓ | ✓ | ✗ | ✗ | Raw float32, no header |
| `binary_header` | ✅ Always | Native C++ | ✗ | ✓ | ✗ | ✗ | Float32 with shape header |
| `mmap` | ✅ Always | Native C++ | ✓ | ✓ | ✗ | ✗ | Memory-mapped binary (POSIX) |
| `direct_io` | ✅ Linux | Native C++ | ✓ | ✓ | ✗ | ✗ | O_DIRECT — bypass page cache |
| `rsf` | ✅ Always | Native C++ | ✓ | ✓ | ✗ | ✗ | Madagascar Regularly Sampled Format |
| `segd` | ✅ Always | Native C++ | ✗ | ✓ | ✓ | ✓ | SEG-D field recording format |
| `segy` | ✅ Always | Native C++ | ✗ | ✗ | ✓ | ✓ | SEG-Y seismic format |
| `npy` | ✅ Always | Native C++ | ✗ | ✓ | ✗ | ✗ | NumPy native format via cnpy |
| `json` | ✅ Always | Native C++ | ✗ | ✓ | ✗ | ✗ | JSON 2D array (nlohmann/json) |
| `hdf5` | ✅ Optional | Native C++ | ✓ | ✗ | ✗ | ✗ | HDF5 via HighFive |
| `netcdf` | ✅ Optional | Native C++ | ✗ | ✗ | ✗ | ✗ | NetCDF4 C++ |
| `parquet` | ✅ Optional | Native C++ | ✗ | ✗ | ✗ | ✗ | Apache Parquet via Arrow |
| `tiledb` | ✅ Optional | Native C++ | ✓ | ✗ | ✗ | ✗ | TileDB dense array |
| `zarr` | ✅ Optional | Native C++ | ✓ | ✗ | ✗ | ✗ | Zarr v2 via native chunked binary + JSON |
| `duckdb` | ✅ Optional | Native C++ | ✓ | ✗ | ✗ | ✗ | DuckDB columnar SQL engine |
| `mdio` | ✅ Optional | Python bridge | ✗ | ✗ | ✗ | ✗ | MDIO for seismic |
| `miniseed` | ✅ Optional | Python bridge | ✗ | ✗ | ✗ | ✗ | MiniSEED via obspy |
| `asdf` | ✅ Optional | Python bridge | ✗ | ✗ | ✗ | ✗ | ASDF via pyasdf |
| `tensorstore` | ✅ Optional | Native C++ | ✓ | ✓ | ✗ | ✗ | TensorStore C++ (zarr driver) |
| `adios2` | ❌ N/A | — | ✗ | ✗ | ✗ | ✗ | ADIOS2 BP (no library) |

## Geophysics Presets

Built-in benchmark scenarios for typical geophysics workloads:

| Preset | Grid | Size | Description |
|--------|------|------|-------------|
| `2d-survey-line` | 480 × 1501 | 2.7 MB | 2D marine seismic survey line |
| `2d-velocity-model` | 401 × 201 | 0.3 MB | 2D velocity model for RTM |
| `3d-velocity-model` | 401 × 201 × 201 | 62 MB | 3D velocity model for RTM |
| `3d-large-survey` | 600 × 400 × 300 | 275 MB | Large 3D survey volume |
| `3d-big-volume` | 401 × 401 × 501 | 307 MB | Big 3D volume for throughput testing |
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
  --threads <n>         Parallel read threads (default: 1, sequential)
  --slice-read          Run inline slice read benchmark (requires 3D, ny>1)
  --trace-read          Run trace read benchmark (sequential + random trace access)
  --stream-write        Run streaming write benchmark (append-only trace writes)
  --checkpoint          Run checkpoint/restart benchmark (write-then-read with integrity check)
  --big-volume          Shortcut for --preset 3d-big-volume (401×401×501, ~307 MB)
  --compression-sweep   Run compression level sweep (levels 0-9)
  --all                 Run all benchmark modes (slice, trace, stream, checkpoint, compression)
  --preset <name>       Use geophysics preset (overrides nx/nz/ny/iterations)
  --list-presets        List available geophysics presets
  --output <path>       Output markdown report path
  --help                Show help
```

### Benchmark Modes

1. **Sequential** (default): Write + read each format, measure throughput
2. **Parallel Read** (`--threads N`): Multiple threads reading same file simultaneously — measures concurrent I/O scaling
3. **Slice Read** (`--slice-read`): Read single inline from 3D volume vs full read — measures chunking/direct access efficiency
4. **Trace Read** (`--trace-read`): Sequential + random trace access — measures trace-level random access
5. **Streaming Write** (`--stream-write`): Trace-by-trace append vs bulk write — measures streaming overhead
6. **Checkpoint/Restart** (`--checkpoint`): Write-then-read with integrity verification — measures round-trip reliability
7. **Compression Sweep** (`--compression-sweep`): Levels 0-9 — measures compression ratio vs throughput tradeoff
8. **All Modes** (`--all`): Run all benchmark modes with a 3D volume

### Examples

```bash
# Small test
./build/io_bench --nx 50 --nz 40

# 3D benchmark with compression ratio
./build/io_bench --nx 100 --nz 80 --ny 50 --iterations 3

# Geophysics presets
./build/io_bench --preset 2d-survey-line
./build/io_bench --preset 3d-velocity-model
./build/io_bench --preset shot-gather

# Parallel read benchmark (4 threads)
./build/io_bench --nx 200 --nz 100 --ny 50 --threads 4

# Inline slice read benchmark (measures chunking efficiency)
./build/io_bench --preset 3d-velocity-model --slice-read

# Production benchmark with report
./build/io_bench --preset 3d-velocity-model --output results.md
```

## Key Results

### Sequential Read Throughput (typical)
- **binary_f32 / mmap**: 1000–5000 MB/s (cached), 2229 MB/s (O_DIRECT)
- **TensorStore C++**: ~2905 MB/s read, 229 MB/s write (zarr+blosc)
- **RSF**: 800–5000 MB/s (native C++, direct seek)
- **HDF5**: 150–200 MB/s
- **Zarr**: 50–100 MB/s (native C++, chunked filesystem)
- **MDIO**: 50–100 MB/s (Python bridge overhead)

### Slice Read Efficiency (3D volume)
- **Native slice (binary, mmap, RSF, TensorStore)**: ~196x speedup vs full volume read
- **Fallback (Python bridge)**: 0.8–1.0x (no benefit — reads entire file)

### Compression Ratio
- **Uncompressed** (binary, mmap, npy, RSF): 1.00:1
- **MDIO** (Zarr+Blosc): 1.11:1 (smaller than raw on some datasets)
- **SEG-Y**: 0.54:1 (trace header overhead)
- **JSON**: 0.24:1 (4x bloat — text encoding)

See `docs/FORMAT_FEATURES.md` for the complete feature matrix.

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
- Slice read correctness
- Direct I/O functionality
- Thread safety contracts
- Compression ratio validation

## License

MIT
