# RFC: I/O Format Selection for RTM Workflows (C++ Implementation)

**Status:** Draft  
**Author:** io-bench-cpp benchmark  
**Date:** 2026-04-07 (updated)  
**Related:** Python benchmark results (rtm3d-cli/scripts/io_format_benchmark.py)

## Summary

This RFC presents a comprehensive I/O format benchmark implemented in C++ to guide format selection for high-performance RTM (Reverse Time Migration) workflows. The benchmark compares throughput characteristics across multiple storage formats commonly used in scientific computing.

## Motivation

Previous benchmark work in Python (rtm3d-cli) provided valuable insights, but C++ implementation offers:
1. Lower-level access to I/O primitives (mmap, direct I/O)
2. Better representation of production RTM code paths
3. Elimination of Python interpreter overhead
4. Direct comparison with the actual RTM implementation language

## Hardware Specification

The benchmarks were run on the following hardware configuration:

### CPU
| Parameter | Value |
|-----------|-------|
| Model | AMD EPYC 7543P 32-Core Processor |
| Cores | 2 (allocated) |
| Threads | 1 per core |
| Architecture | x86_64 |
| Clock | ~2.8 GHz (base) |

### Memory
| Parameter | Value |
|-----------|-------|
| Total RAM | 7.8 GiB |
| Available | 6.7 GiB |
| Type | DDR4 ECC |
| Swap | 0 B |

### Storage
| Parameter | Value |
|-----------|-------|
| Type | Overlay filesystem (Docker container) |
| Backend | SSD (estimated) |
| Total | 96 GB |
| Available | 77 GB |
| Mount | / (root) |

### Operating System
| Parameter | Value |
|-----------|-------|
| Kernel | Linux 6.8.0-94-generic |
| Distribution | Ubuntu 22.04 (container) |
| Architecture | x86_64 |

### Benchmark Configuration
| Parameter | Value |
|-----------|-------|
| Compiler | g++ 13 |
| Optimization | -O3 |
| C++ Standard | C++20 |

## Format Coverage

### Implemented and Working (13/14)

| Format | Status | 2D | 3D | Implementation | Notes |
|--------|--------|----|----|----------------|-------|
| binary_f32 | ✅ | ✅ | ✅ | Native | Raw float32, no header |
| binary_header | ✅ | ✅ | ✅ | Native | Self-describing with dims |
| mmap | ✅ | ✅ | ✅ | POSIX mmap | Zero-copy, OS-managed |
| npy | ✅ | ✅ | ✅ | cnpy | NumPy native format |
| json | ✅ | ✅ | ✅ | nlohmann/json | Human-readable (slow) |
| hdf5 | ✅ | ✅ | ✅ | HighFive/C API | Scientific standard |
| netcdf | ✅ | ✅ | ✅ | libnetcdf | Climate/CF conventions |
| tiledb | ✅ | ✅ | ✅ | libtiledb | Cloud arrays |
| zarr | ✅ | ✅ | ✅ | Native C++ | Chunked binary + JSON |
| parquet | ✅ | ✅ | ✅ | Apache Arrow | Columnar storage |
| segy | ✅ | ✅ | ✅ | Native C++ | SEG-Y rev1 |
| duckdb | ✅ | ✅ | ✅ | libduckdb | SQL database |
| tensorstore | ✅ | ✅ | ✅ | Python bridge | Google TensorStore via zarr |
| mdio | ❌ | - | - | N/A | No C++ library available |

## Benchmark Results

### Test Configuration

| Parameter | Small | Medium | Large |
|-----------|-------|--------|-------|
| Grid size | 80×100 | 400×300 | 800×600 |
| Data size | 0.031 MB | 0.458 MB | 1.83 MB |
| Iterations | 3 | 5 | 10 |
| Compiler | g++ -O3 -std=c++20 |

### Throughput Results (Medium: 400×300 float32, 2D)

| Format | Write MB/s | Read MB/s | Size (MB) | Notes |
|--------|-----------|-----------|-----------|-------|
| **binary_header** | 265.5 | **6275.1** | 0.458 | Best read throughput |
| **npy** | **275.3** | 2705.9 | 0.458 | Best write, portable |
| **mmap** | 223.5 | 2831.6 | 0.458 | Zero-copy, OS-managed |
| **hdf5** | 152.0 | 1839.0 | 0.460 | Self-describing, metadata |
| **binary_f32** | 248.6 | 1578.8 | 0.458 | Simplest, no header |
| **netcdf** | 96.2 | 675.8 | 0.458 | CF conventions |
| **parquet** | 44.3 | 407.8 | 0.824 | Columnar, 1.8× size |
| **segy** | 119.1 | 134.1 | 0.553 | Industry standard |
| **zarr** | 42.3 | 96.6 | 0.458 | Chunked, cloud-ready |
| **tiledb** | 16.3 | 115.4 | 0.462 | Cloud-native |
| **json** | 69.1 | 37.4 | 1.914 | 4.2× size overhead |
| **tensorstore** | 1.1 | 1.5 | 0.415 | Python subprocess overhead |

### Key Observations

1. **Binary formats dominate**: Raw binary variants achieve 1579-6275 MB/s read, 249-275 MB/s write
2. **Memory-mapped I/O**: Excellent read (2832 MB/s) with zero-copy semantics
3. **Parquet competitive**: 408 MB/s read with columnar structure, 1.8× size overhead
4. **Zarr viable**: 97 MB/s read in native C++, cloud-compatible
5. **TensorStore overhead**: Python bridge adds significant latency (1-2 MB/s), suitable only for offline workflows
6. **JSON overhead**: 4.2× size bloat, 168× slower read than binary_header
7. **TileDB slow write**: 16.3 MB/s write, but reasonable read at 115 MB/s

### C++ vs Python Comparison

| Format | C++ Read MB/s | Python Read MB/s | Speedup |
|--------|--------------|------------------|---------|
| binary_f32 | 1578.8 | 309.0 | **5.1×** |
| npy | 2705.9 | 88.0 | **30.8×** |
| hdf5 | 1839.0 | 23.0 | **79.9×** |
| netcdf | 675.8 | 27.0 | **25.0×** |
| json | 37.4 | 27.0 | 1.4× |
| parquet | 407.8 | 42.0 | **9.7×** |
| zarr | 96.6 | 31.0 | **3.1×** |

C++ implementation shows dramatic improvements for binary and scientific formats due to:
- Direct memory access without Python object creation
- No NumPy array allocation overhead
- Compiler optimizations (SIMD, loop unrolling)
- Lower-level C library bindings (HDF5, NetCDF C APIs)

## Format Recommendations

### Tier 1: Production Hot Paths

| Use Case | Recommended Format | Rationale |
|----------|-------------------|-----------|
| Velocity model loading | `binary_header` or `mmap` | Maximum throughput, metadata in header |
| RTM checkpoint/restart | `binary_f32` | Simplest, fastest write |
| Seismic trace data | `segy` | Industry standard (when available) |

### Tier 2: Interoperability

| Use Case | Recommended Format | Rationale |
|----------|-------------------|-----------|
| NumPy ecosystem | `npy` | Native format, best write throughput |
| MATLAB users | `npy` | scipy.io.loadmat alternative |
| Self-describing | `hdf5` | Rich metadata, hierarchical, excellent read |
| Climate/science | `netcdf` | CF conventions, metadata |
| Analytics pipelines | `parquet` | Columnar, good read (408 MB/s) |

### Tier 3: Cloud/Distributed

| Use Case | Recommended Format | Rationale |
|----------|-------------------|-----------|
| Cloud object storage | `zarr` | Chunked, parallel access |
| Cloud-native arrays | `tiledb` | Cloud-optimized, good read |
| Offline tensor access | `tensorstore` | Python bridge, for non-hot paths |

### Not Recommended

| Format | Issue |
|--------|-------|
| `json` | 4× size overhead, 168× slower read than binary_header |
| `duckdb` (row insert) | Extremely slow for array data |
| `tensorstore` (hot path) | Python subprocess overhead, 1-2 MB/s |

## Implementation Details

### New: Parquet Format (`src/formats/parquet.cpp`)

**Implementation**: Apache Arrow C++ API

**Design**:
- Long-format storage: columns (ix, iy, iz, value) per row
- Uses Arrow builders for efficient column construction
- Apache Arrow 23.0.1 with Parquet support
- 1.8× size overhead vs raw binary (index columns)

### New: TensorStore Format (`src/formats/tensorstore.cpp`)

**Implementation**: Python subprocess bridge

**Design**:
- C++ writes raw binary, invokes Python tensorstore to write zarr
- Read path: Python tensorstore reads zarr, dumps binary, C++ loads
- Uses temporary Python script files to avoid shell quoting issues
- Stores as zarr v2 format via TensorStore's zarr driver

### Updated: Benchmark Directory Support

**Fix**: `benchmark.cpp` now handles directory-based formats (zarr, tensorstore)
- `file_size` uses `recursive_directory_iterator` for directories
- Cleanup uses `remove_all` instead of `remove` for directories

## Performance Analysis

### Read Throughput Hierarchy (400×300)

```
binary_header:  ████████████████████████████████████████████ 6275 MB/s
npy:            ████████████████████████████████ 2706 MB/s
mmap:           ██████████████████████████████ 2832 MB/s
hdf5:           ████████████████████████ 1839 MB/s
binary_f32:     ████████████████████ 1579 MB/s
netcdf:         █████████ 676 MB/s
parquet:        ██████ 408 MB/s
segy:           ██ 134 MB/s
tiledb:         █ 115 MB/s
zarr:           █ 97 MB/s
json:           █ 37 MB/s
tensorstore:    ░ 1.5 MB/s
```

### Write Throughput Hierarchy (400×300)

```
npy:            ████████████████████████████████ 275 MB/s
binary_header:  ██████████████████████████████ 266 MB/s
binary_f32:     ████████████████████████████ 249 MB/s
mmap:           ██████████████████████████ 224 MB/s
hdf5:           ████████████████████ 152 MB/s
netcdf:         ████████████ 96 MB/s
segy:           ████████████ 119 MB/s
json:           ████████ 69 MB/s
parquet:        █████ 44 MB/s
zarr:           █████ 42 MB/s
tiledb:         ██ 16 MB/s
tensorstore:    ░ 1.1 MB/s
```

### Storage Efficiency

```
binary_f32:     ████████████████████████████████████ 100% (0.458 MB)
binary_header:  ████████████████████████████████████ 100% (0.458 MB + 72B)
npy:            ████████████████████████████████████ 100% (0.458 MB + header)
mmap:           ████████████████████████████████████ 100% (0.458 MB)
hdf5:           ████████████████████████████████████ 100% (0.460 MB)
netcdf:         ████████████████████████████████████ 100% (0.458 MB)
tiledb:         ████████████████████████████████████ 101% (0.462 MB)
zarr:           ████████████████████████████████████ 100% (0.458 MB)
segy:           ████████████████████ 121% (0.553 MB)
parquet:        ██████████████████ 180% (0.824 MB)
json:           ████████████████████████████████████████████████████████████████ 418% (1.914 MB)
```

## Dependencies

| Library | Version | Formats | Install |
|---------|---------|---------|---------|
| nlohmann/json | 3.x | json | Bundled (git submodule) |
| cnpy | latest | npy | Bundled (git submodule) |
| HighFive | - | hdf5 | System (linuxbrew) |
| libnetcdf | 22 | netcdf | linuxbrew |
| libtiledb | 2.30 | tiledb | linuxbrew |
| libduckdb | 1.5.1 | duckdb | linuxbrew |
| Apache Arrow | 23.0.1 | parquet | linuxbrew |
| Python tensorstore | 0.1.82 | tensorstore | pip |

## Adoption Roadmap

### Phase 1: Core Formats ✅ COMPLETE
- [x] Implement `binary_f32` read/write
- [x] Implement `binary_header` with metadata
- [x] Implement `mmap` zero-copy reader
- [x] Implement `npy` format via cnpy
- [x] Implement `json` format via nlohmann/json

### Phase 2: Scientific Formats ✅ COMPLETE
- [x] Add HDF5 support (C API)
- [x] Add NetCDF support (C API)
- [x] Add SEG-Y support (native C++)

### Phase 3: Cloud/HPC Formats ✅ COMPLETE
- [x] Add Zarr support (native C++ chunked binary)
- [x] Add TileDB support (libtiledb)
- [x] Add Parquet support (Apache Arrow)
- [x] Add DuckDB support (libduckdb)
- [x] Add TensorStore support (Python bridge)

## Conclusions

1. **Binary formats are essential** for production RTM workflows—JSON is impractical for large arrays
2. **Memory-mapped I/O** provides the best combination of performance and code simplicity for read-heavy workloads
3. **C++ offers 5-80× improvement** over Python for binary format throughput
4. **NPY format** is the recommended choice for NumPy interoperability with excellent performance
5. **Parquet** is competitive for read-heavy analytics workflows (408 MB/s read) despite columnar overhead
6. **TensorStore via Python bridge** is too slow for hot paths but viable for offline tensor access
7. **Self-describing formats** (HDF5, NetCDF) should be reserved for metadata-rich workflows where the overhead is justified

## References

- Python benchmark: `rtm3d-cli/scripts/io_format_benchmark.py`
- NPY format spec: https://numpy.org/doc/stable/reference/generated/numpy.lib.format.html
- HDF5: https://www.hdfgroup.org/solutions/hdf5/
- NetCDF: https://www.unidata.ucar.edu/software/netcdf/
- Apache Arrow/Parquet: https://arrow.apache.org/
- TensorStore: https://google.github.io/tensorstore/
