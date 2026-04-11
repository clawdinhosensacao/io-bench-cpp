# io-bench-cpp Development Report

## Executive Summary

This document describes the implementation of I/O format adapters in the `io-bench-cpp` project, dependency management, and the current state of format coverage.

---

## 1. Format Coverage

### Current State (2026-04-11)

| Format | Status | Implementation | Slice | Thread-safe | Trace | Stream | Compression |
|--------|--------|----------------|-------|-------------|-------|--------|-------------|
| binary_f32 | ✅ Always | Native C++ | ✓ | ✓ | ✗ | ✗ | ✗ |
| binary_header | ✅ Always | Native C++ | ✗ | ✓ | ✗ | ✗ | ✗ |
| mmap | ✅ Always | Native C++ | ✓ | ✓ | ✗ | ✗ | ✗ |
| direct_io | ✅ Linux | Native C++ | ✓ | ✓ | ✗ | ✗ | ✗ |
| rsf | ✅ Always | Native C++ | ✓ | ✓ | ✗ | ✗ | ✗ |
| segd | ✅ Always | Native C++ | ✗ | ✓ | ✓ | ✓ | ✗ |
| segy | ✅ Always | Native C++ | ✗ | ✗ | ✓ | ✓ | ✗ |
| npy | ✅ Always | Native C++ (cnpy) | ✗ | ✓ | ✗ | ✗ | ✗ |
| json | ✅ Always | Native C++ (nlohmann/json) | ✗ | ✓ | ✗ | ✗ | ✗ |
| hdf5 | ✅ Optional | Native C++ (HighFive) | ✗ | ✗ | ✗ | ✗ | ✗ |
| netcdf | ✅ Optional | Native C++ (libnetcdf) | ✗ | ✗ | ✗ | ✗ | ✗ |
| tiledb | ✅ Optional | Native C++ (libtiledb) | ✗ | ✗ | ✗ | ✗ | ✗ |
| duckdb | ✅ Optional | Native C++ (libduckdb) | ✗ | ✗ | ✗ | ✗ | ✗ |
| parquet | ✅ Optional | Native C++ (Arrow/Parquet) | ✗ | ✗ | ✗ | ✗ | ✗ |
| zarr | ✅ Optional | Python bridge | ✗ | ✗ | ✗ | ✗ | ✗ |
| mdio | ✅ Optional | Python bridge | ✗ | ✗ | ✗ | ✗ | ✗ |
| miniseed | ✅ Optional | Python bridge (obspy) | ✗ | ✗ | ✗ | ✗ | ✗ |
| asdf | ✅ Optional | Python bridge (pyasdf) | ✗ | ✗ | ✗ | ✗ | ✗ |
| tensorstore | ✅ Optional | **Native C++** (zarr driver) | ✓ | ✓ | ✗ | ✗ | ✓ blosc-lz4 |
| adios2 | ❌ N/A | Not available | ✗ | ✗ | ✗ | ✗ | ✗ |

**Total: 19 formats (18 working)**

---

## 2. Dependency Management

### 2.1 Installed Dependencies (via Homebrew/linuxbrew)

```bash
# Core dependencies (always required)
hdf5          # HDF5 format
netcdf        # NetCDF4 format
tiledb        # TileDB format
duckdb        # DuckDB format
adios2        # ADIOS2 format (installed but untested)
apache-arrow   # Parquet format

# TensorStore (built from source via CMake)
# Installed to /data/.local
tensorstore   # Native C++ API, zarr driver with blosc compression
```

### 2.2 Build System Configuration

The Makefile uses automatic dependency detection with proper runtime linking:

```makefile
# Runtime library path configuration
HDF5_PREFIX := /home/linuxbrew/.linuxbrew

ifeq ($(HDF5_EXISTS),yes)
CXXFLAGS += -DHAVE_HDF5 -I$(HDF5_PREFIX)/include
LDFLAGS += -L$(HDF5_PREFIX)/lib -lhdf5 -lhdf5_hl \
           -Wl,-rpath,$(HDF5_PREFIX)/lib \
           -Wl,-rpath-link,$(HDF5_PREFIX)/lib
endif
```

Key practices:
1. **`-Wl,-rpath`**: Embeds runtime library search path in binary
2. **`-Wl,-rpath-link`**: Ensures linker can resolve transitive dependencies at build time
3. **Conditional compilation**: `#ifdef HAVE_*` guards for optional formats
4. **TensorStore alwayslink**: Uses `--whole-archive` for driver auto-registration

---

## 3. Key Format Implementations

### 3.1 TensorStore C++ Native (`src/formats/tensorstore_native.cpp`)

**Implementation**: Native C++ using TensorStore's `tensorstore::Open/Read/Write` API with zarr driver

**Key results**:
- Read: ~2905 MB/s (was ~1 MB/s with Python bridge — ~2900x improvement)
- Write: ~229 MB/s
- Supports slice reads, compression sweep (blosc-lz4 levels 0-9)
- Thread-safe (read-only concurrent access)

**Dual implementation**:
- `HAVE_TENSORSTORE_CPP` defined → native C++ API (name: `tensorstore_cpp`)
- Otherwise → Python bridge fallback (name: `tensorstore`)

### 3.2 SEG-Y Format (`src/formats/segy.cpp`)

**Implementation**: Native C++ SEG-Y rev1 compliant

**Design**:
- 3200-byte text header (spaces)
- 400-byte binary header with sample count and format code
- 240-byte trace headers per trace
- IEEE float samples (format code 5) in big-endian
- Supports trace-based random access (`read_trace`)
- Supports streaming append writes (`write_trace`)

### 3.3 SEG-D Format (`src/formats/segd.cpp`)

**Implementation**: Native C++ simplified SEGD Rev 3.0

**Design**:
- 96-byte general header block with file ID, format code, trace/sample counts
- 20-byte trace headers per trace
- IEEE float32 samples in big-endian (portable byte-swap, no compiler intrinsics)
- Supports trace-based random access and streaming append

### 3.4 RSF Format (`src/formats/rsf.cpp`)

**Implementation**: Native C++ for Madagascar's Regularly Sampled Format

**Design**:
- Directory-based layout: `header.rsf` (text key=value pairs) + `data.bin` (raw float32)
- Native slice read via file seek (offset calculation)
- RSF convention: n1=fastest axis (x), n2=z, n3=y

### 3.5 Direct I/O (`src/formats/binary.cpp` — `DirectIOFormat`)

**Implementation**: Native C++ using Linux `O_DIRECT`

**Design**:
- Bypasses page cache for true disk throughput measurement
- Requires aligned memory (`posix_memalign`) and aligned I/O offsets
- Auto-truncates padding after aligned write
- Native slice read with aligned offset calculation

### 3.6 Zarr Format (`src/formats/zarr.cpp`)

**Implementation**: Native C++ using filesystem + nlohmann/json

**Design**:
- Creates Zarr v2 compatible directory structure
- `.zarray` metadata file (JSON) with shape/chunks/dtype
- Chunked binary files
- No compression (raw chunks) for baseline performance

---

## 4. Benchmark Modes

| Mode | CLI Flag | Description |
|------|----------|-------------|
| Sequential | (default) | Write + read each format, measure throughput |
| Parallel Read | `--threads N` | Multi-threaded concurrent reads |
| Slice Read | `--slice-read` | Read single inline from 3D volume vs full read |
| Trace Read | `--trace-read` | Sequential + random trace access |
| Streaming Write | `--stream-write` | Trace-by-trace append vs bulk write |
| Checkpoint/Restart | `--checkpoint` | Write-then-read-back with integrity verification |
| Compression Sweep | `--compression-sweep` | Compression levels 0-9 |
| All Modes | `--all` | Run all modes with 3D volume |

---

## 5. Build and Test

### 5.1 Build Commands

```bash
make fetch-deps       # Fetch third-party deps (nlohmann/json, cnpy, googletest)
make build/io_bench   # Build benchmark executable
make test             # Build and run tests
make bench            # Run benchmark with default grid
```

### 5.2 Test Results

```
[==========] 57 tests from 3 test suites ran.
[  PASSED  ] 57 tests.
```

---

## 6. Geophysics Presets

| Preset | Grid | Size | Description |
|--------|------|------|-------------|
| `2d-survey-line` | 480 × 1501 | 2.7 MB | 2D marine seismic survey line |
| `2d-velocity-model` | 401 × 201 | 0.3 MB | 2D velocity model for RTM |
| `3d-velocity-model` | 401 × 201 × 201 | 62 MB | 3D velocity model for RTM |
| `3d-large-survey` | 600 × 400 × 300 | 275 MB | Large 3D survey volume |
| `3d-big-volume` | 401 × 401 × 501 | 307 MB | Big 3D volume for throughput testing |
| `shot-gather` | 640 × 4001 | 9.8 MB | Single shot gather (traces × time) |
| `checkpoint-restart` | 200 × 100 × 100 | 7.6 MB | RTM checkpoint/restart volume |

---

## 7. Future Work

### Not Yet Implemented
- **Cloud I/O Benchmark**: S3/GCS read performance (requires cloud credentials)
- **OpenVDS** (Equinor): Cloud-optimized seismic, C++ native (complex API, on hold)
- **DL-SEG-Y**: Deep-learning variant of SEG-Y
- **BP/BLOOM**: ADIOS2-based formats for HPC seismic

### Known Limitations
1. **DuckDB**: Row-oriented storage is slow for large arrays; consider column-oriented approach
2. **Zarr (Python bridge)**: No compression support in native C++ implementation
3. **TileDB/ADIOS2**: Available but not fully tested with large datasets
4. **Python bridge formats**: High overhead (~1 MB/s) due to subprocess spawning; TensorStore C++ native is the exception
