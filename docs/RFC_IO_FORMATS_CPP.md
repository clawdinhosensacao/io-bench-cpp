# RFC: I/O Format Selection for RTM Workflows (C++ Implementation)

**Status:** Draft  
**Author:** io-bench-cpp benchmark  
**Date:** 2026-04-05  
**Related:** Python benchmark results (rtm3d-cli/scripts/io_format_benchmark.py)

## Summary

This RFC presents a comprehensive I/O format benchmark implemented in C++ to guide format selection for high-performance RTM (Reverse Time Migration) workflows. The benchmark compares throughput characteristics across multiple storage formats commonly used in scientific computing.

## Motivation

Previous benchmark work in Python (rtm3d-cli) provided valuable insights, but C++ implementation offers:
1. Lower-level access to I/O primitives (mmap, direct I/O)
2. Better representation of production RTM code paths
3. Elimination of Python interpreter overhead
4. Direct comparison with the actual RTM implementation language

## Benchmark Results

### Test Configuration

| Parameter | Value |
|-----------|-------|
| Grid size | 400 × 300 float32 |
| Data size | 0.458 MB |
| Iterations | 5 |
| Compiler | g++ -O3 -std=c++20 |

### Throughput Results

| Format | Write MB/s | Read MB/s | Size (MB) | Notes |
|--------|-----------|-----------|-----------|-------|
| **binary_header** | 253.7 | **4690.3** | 0.458 | Best read throughput |
| **mmap** | 96.2 | 2989.2 | 0.458 | Zero-copy, OS-managed |
| **npy** | 232.4 | 2503.7 | 0.458 | NumPy native, portable |
| **binary_f32** | **281.9** | 1567.7 | 0.458 | Simplest, no header |
| **json** | 63.6 | 32.4 | 1.914 | 4.2× size overhead |

### Key Observations

1. **Binary formats dominate**: Raw binary variants achieve 1500-4700 MB/s read, 250-280 MB/s write
2. **Memory-mapped I/O**: Excellent read (2989 MB/s) with zero-copy semantics
3. **JSON overhead**: 4.2× size bloat, 24× slower read than binary_f32
4. **NPY format**: Good balance of portability and performance (2500 MB/s read)

### C++ vs Python Comparison

| Format | C++ Read MB/s | Python Read MB/s | Speedup |
|--------|--------------|------------------|---------|
| binary_f32 | 1567.7 | 309.0 | **5.1×** |
| npy | 2503.7 | 88.0 | **28.5×** |
| json | 32.4 | 27.0 | 1.2× |

C++ implementation shows dramatic improvements for binary and NPY formats due to:
- Direct memory access without Python object creation
- No NumPy array allocation overhead
- Compiler optimizations (SIMD, loop unrolling)

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
| NumPy ecosystem | `npy` | Native format, good performance |
| MATLAB users | `npy` | scipy.io.loadmat alternative |
| Self-describing | `hdf5` | Rich metadata, hierarchical |
| Climate/science | `netcdf` | CF conventions, metadata |

### Tier 3: Cloud/Distributed

| Use Case | Recommended Format | Rationale |
|----------|-------------------|-----------|
| Cloud object storage | `zarr` | Chunked, parallel access |
| Analytics pipelines | `parquet` | Columnar, compression |
| HPC parallel I/O | `adios2` | MPI-IO, streaming |

### Not Recommended

| Format | Issue |
|--------|-------|
| `json` | 4× size overhead, 50× slower than binary |
| `duckdb` (row insert) | Extremely slow for array data (26s for 8K values) |

## Implementation Guidance

### Binary Header Format

```cpp
struct BinaryHeader {
    uint32_t magic = 0x52544D33;  // "RTM3"
    uint32_t version = 1;
    uint32_t nx, nz, ny;
    float dx, dz, dy;
    uint32_t reserved[8];
};  // 72 bytes total
```

Benefits:
- Self-describing (dimensions embedded)
- Backward compatible version field
- Reserved space for future metadata

### Memory-Mapped I/O

```cpp
#include <sys/mman.h>
#include <fcntl.h>

class MappedReader {
    int fd_;
    void* data_;
    size_t size_;
public:
    MappedReader(const std::string& path) {
        fd_ = open(path.c_str(), O_RDONLY);
        size_ = lseek(fd_, 0, SEEK_END);
        data_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    }
    ~MappedReader() {
        munmap(data_, size_);
        close(fd_);
    }
    const float* data() const { return static_cast<const float*>(data_); }
};
```

Benefits:
- Zero-copy: OS pages directly to application
- Lazy loading: Only touched pages fault in
- Scalable: Works for multi-GB models

### NPY Format Integration

The `cnpy` library provides header-only C++ NPY support:

```cpp
#include "cnpy.h"

// Write
std::vector<size_t> shape = {nz, nx};
cnpy::npy_save("output.npy", data, shape, "w");

// Read
cnpy::NpyArray arr = cnpy::npy_load("output.npy");
float* data = arr.data<float>();
```

## Performance Characteristics

### Read Throughput Hierarchy

```
binary_header:  ████████████████████████████████████████████ 4690 MB/s
mmap:           ████████████████████████████████ 2989 MB/s
npy:            ████████████████████████████ 2504 MB/s
binary_f32:     ████████████████████ 1568 MB/s
json:           █ 32 MB/s
```

### Write Throughput Hierarchy

```
binary_f32:     ████████████████████████████████████ 282 MB/s
binary_header:  ████████████████████████████████ 254 MB/s
npy:            ████████████████████████████ 232 MB/s
mmap:           ████████████ 96 MB/s
json:           ████████ 64 MB/s
```

### Storage Efficiency

```
binary_f32:     ████████████████████████████████████ 100% (0.458 MB)
binary_header:  ████████████████████████████████████ 100% (0.458 MB + 72B)
npy:            ████████████████████████████████████ 100% (0.458 MB + header)
mmap:           ████████████████████████████████████ 100% (0.458 MB)
json:           ████████████████████████████████████████████████████████████████ 418% (1.914 MB)
```

## Adoption Roadmap

### Phase 1: Core Formats (Immediate)
- [x] Implement `binary_f32` read/write
- [x] Implement `binary_header` with metadata
- [x] Implement `mmap` zero-copy reader
- [x] Implement `npy` format via cnpy

### Phase 2: Scientific Formats (Near-term)
- [ ] Add HDF5 support (HighFive library)
- [ ] Add NetCDF support (netcdf-cxx4)
- [ ] Add SEG-Y support (segyio-cpp or custom)

### Phase 3: Cloud/HPC Formats (Future)
- [ ] Evaluate Zarr C++ implementation
- [ ] Evaluate ADIOS2 for parallel I/O
- [ ] Evaluate TileDB for cloud arrays

## Conclusions

1. **Binary formats are essential** for production RTM workflows—JSON is impractical for large arrays
2. **Memory-mapped I/O** provides the best combination of performance and code simplicity for read-heavy workloads
3. **C++ offers 5-30× improvement** over Python for binary format throughput
4. **NPY format** is the recommended choice for NumPy interoperability with excellent performance
5. **Self-describing formats** (HDF5, NetCDF) should be reserved for metadata-rich workflows where the overhead is justified

## Appendix: Benchmark Methodology

### Timing Approach

```cpp
auto start = std::chrono::high_resolution_clock::now();
// ... I/O operation ...
auto end = std::chrono::high_resolution_clock::now();
double ms = std::chrono::duration<double, std::milli>(end - start).count();
```

### Throughput Calculation

```cpp
double mb = static_cast<double>(nx * nz * sizeof(float)) / (1024.0 * 1024.0);
double mbps = mb / (ms / 1000.0);
```

### Statistical Treatment

- 5 iterations per measurement
- First iteration discarded as warmup (optional)
- Report median of remaining iterations
- 95% confidence intervals computed for large datasets

## References

- Python benchmark: `rtm3d-cli/scripts/io_format_benchmark.py`
- NPY format spec: https://numpy.org/doc/stable/reference/generated/numpy.lib.format.html
- HDF5: https://www.hdfgroup.org/solutions/hdf5/
- NetCDF: https://www.unidata.ucar.edu/software/netcdf/
