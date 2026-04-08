# RFC: I/O Format Selection for Seismic & Geophysics Workflows (C++ Implementation)

**Status:** Draft  
**Author:** io-bench-cpp benchmark  
**Date:** 2026-04-08 (updated)  
**Related:** Python benchmark results (rtm3d-cli/scripts/io_format_benchmark.py)

## Summary

This RFC presents a comprehensive I/O format benchmark implemented in C++ to guide format selection for high-performance seismic and geophysics workflows. The benchmark compares throughput characteristics across **15 storage formats** — including geophysics-native formats (SEG-Y, MDIO, MiniSEED) — with built-in geophysics preset scenarios.

## Motivation

Previous benchmark work in Python (rtm3d-cli) provided valuable insights, but C++ implementation offers:
1. Lower-level access to I/O primitives (mmap, direct I/O)
2. Better representation of production RTM code paths
3. Elimination of Python interpreter overhead
4. Direct comparison with the actual RTM implementation language

### Why Geophysics-Specific Benchmarking?

Seismic workflows have unique I/O characteristics:
- **Large 3D volumes**: velocity models (401×201×201 ≈ 62 MB), migration outputs (GB-scale)
- **Trace-based access**: shot gathers stored as sequential traces (SEG-Y, MiniSEED)
- **Cloud-native shift**: MDIO, Zarr, TileDB for cloud object storage
- **Checkpoint/restart**: RTM needs fast full-volume write + read-back for fault tolerance

## Hardware Specification

| Parameter | Value |
|-----------|-------|
| CPU | AMD EPYC 7543P 32-Core (2 cores allocated) |
| RAM | 7.8 GiB DDR4 ECC |
| Storage | Overlay filesystem (Docker container, SSD backend) |
| OS | Ubuntu 22.04 (container), Linux 6.8.0-94-generic |
| Compiler | g++ 13, -O3, C++20 |

## Format Coverage

### Implemented and Working (14/15)

| Format | 2D | 3D | Implementation | Geophysics Relevance |
|--------|----|----|----------------|---------------------|
| binary_f32 | ✅ | ✅ | Native C++ | Velocity model, checkpoint |
| binary_header | ✅ | ✅ | Native C++ | Self-describing velocity |
| mmap | ✅ | ✅ | POSIX mmap | Zero-copy wavefield access |
| npy | ✅ | ✅ | cnpy | NumPy ecosystem interop |
| json | ✅ | ✅ | nlohmann/json | Config/metadata only |
| hdf5 | ✅ | ✅ | HighFive/C API | SEGY-derived, ASDF base |
| netcdf | ✅ | ✅ | libnetcdf | Climate, CF conventions |
| tiledb | ✅ | ✅ | libtiledb | Cloud-native arrays |
| zarr | ✅ | ✅ | Native C++ | Cloud chunked storage |
| parquet | ✅ | ✅ | Apache Arrow | Analytics pipelines |
| segy | ✅ | ✅ | Native C++ | **Seismic industry standard** |
| duckdb | ✅ | ✅ | libduckdb | SQL analytics on traces |
| tensorstore | ✅ | ✅ | Python bridge | ML tensor storage |
| mdio | ✅ | ✅ | Python bridge | **Cloud-native seismic (TGS)** |
| miniseed | ✅ | ✅ | Python bridge | **Seismological time series** |
| adios2 | ❌ | ❌ | N/A | No library available |

## Geophysics Benchmark Presets

Built-in scenarios for typical geophysics workloads:

| Preset | Grid | Size | Use Case |
|--------|------|------|----------|
| `2d-survey-line` | 480×1501 | 2.7 MB | 2D marine seismic line |
| `2d-velocity-model` | 401×201 | 0.3 MB | 2D velocity model for RTM |
| `3d-velocity-model` | 401×201×201 | 62 MB | 3D velocity model for RTM |
| `3d-large-survey` | 600×400×300 | 275 MB | Large 3D survey volume |
| `shot-gather` | 640×4001 | 9.8 MB | Single shot gather |
| `checkpoint-restart` | 200×100×100 | 7.6 MB | RTM checkpoint/restart |

## Benchmark Results

### Shot Gather Preset (640×4001, ~10 MB, 5 iterations)

| Format | Write MB/s | Read MB/s | Size (MB) | Category |
|--------|-----------|-----------|-----------|----------|
| **hdf5** | 394.8 | **4076.9** | 9.770 | Scientific |
| **binary_header** | 367.3 | 3894.7 | 9.768 | Binary |
| **mmap** | 372.0 | 2842.7 | 9.768 | Binary |
| **npy** | 386.2 | 1950.5 | 9.768 | Binary |
| **binary_f32** | 339.1 | 1016.8 | 9.768 | Binary |
| **netcdf** | 240.8 | 1107.3 | 9.768 | Scientific |
| **parquet** | 59.2 | 484.3 | 14.502 | Columnar |
| **tiledb** | 115.3 | 447.3 | 9.774 | Cloud |
| **segy** | 120.6 | 158.5 | 9.918 | **Seismic** |
| **zarr** | 57.2 | 114.0 | 9.768 | Cloud |
| **miniseed** | 17.0 | 17.4 | 9.906 | **Seismology** |
| **json** | 64.9 | 36.7 | 40.826 | Text |
| **tensorstore** | 6.0 | 26.3 | 8.641 | Python bridge |
| **mdio** | 2.2 | 2.2 | 8.550 | **Seismic (cloud)** |

### 2D Velocity Model Preset (401×201, ~0.3 MB, 3 iterations)

| Format | Write MB/s | Read MB/s | Size (MB) |
|--------|-----------|-----------|-----------|
| binary_header | 198.2 | 4534.0 | 0.307 |
| mmap | 217.8 | 2754.6 | 0.307 |
| npy | 251.3 | 2666.9 | 0.308 |
| hdf5 | 280.6 | 1885.3 | 0.309 |
| binary_f32 | 171.7 | 1475.0 | 0.307 |
| netcdf | 139.9 | 737.1 | 0.308 |
| parquet | 48.3 | 376.6 | 0.552 |
| segy | 127.9 | 107.3 | 0.403 |
| tiledb | 13.7 | 112.8 | 0.312 |
| zarr | 46.2 | 96.9 | 0.308 |
| json | 71.6 | 43.2 | 1.286 |
| miniseed | 0.6 | 0.6 | 0.312 |
| mdio | 0.1 | 0.1 | 0.270 |

### Key Observations

1. **Binary formats dominate**: Raw binary variants achieve 1000-4500 MB/s read for shot gathers
2. **HDF5 best for large reads**: 4077 MB/s read at 10 MB scale — excellent for velocity model loading
3. **SEG-Y competitive at scale**: 120 MB/s write, 159 MB/s read — acceptable for industry standard
4. **MiniSEED 10× faster than MDIO**: 17 MB/s vs 2 MB/s — both via Python bridge, but obspy is leaner
5. **MDIO slowest**: Python subprocess + xarray + zarr stack = 2 MB/s — viable only for offline cloud workflows
6. **Parquet strong read**: 484 MB/s read at 10 MB, despite 1.5× size overhead
7. **JSON impractical**: 4.2× size bloat, 100× slower read than binary_header

### C++ vs Python Comparison (Medium: 400×300)

| Format | C++ Read MB/s | Python Read MB/s | Speedup |
|--------|--------------|------------------|---------|
| binary_f32 | 1578.8 | 309.0 | **5.1×** |
| npy | 2705.9 | 88.0 | **30.8×** |
| hdf5 | 1839.0 | 23.0 | **79.9×** |
| netcdf | 675.8 | 27.0 | **25.0×** |
| parquet | 407.8 | 42.0 | **9.7×** |
| zarr | 96.6 | 31.0 | **3.1×** |

## Format Recommendations by Geophysics Use Case

### RTM Processing (Hot Paths)

| Use Case | Recommended | Alternative | Rationale |
|----------|-------------|-------------|-----------|
| Velocity model load | `binary_header` | `mmap`, `hdf5` | Fastest read, metadata in header |
| RTM checkpoint | `binary_f32` | `npy` | Simplest, fastest write |
| Migration output | `segy` | `hdf5` | Industry standard output |
| Wavefield snapshot | `mmap` | `binary_header` | Zero-copy, OS page cache |

### Seismic Data Management

| Use Case | Recommended | Alternative | Rationale |
|----------|-------------|-------------|-----------|
| Survey archive | `segy` | `mdio` | Industry standard, widest tool support |
| Cloud storage | `mdio` | `zarr` | Seismic-specific chunking + metadata |
| Real-time streams | `miniseed` | — | Seismological standard, trace-based |
| Analytics/ML | `parquet` | `zarr` | Columnar, good read throughput |

### Interoperability

| Use Case | Recommended | Rationale |
|----------|-------------|-----------|
| NumPy ecosystem | `npy` | Native format, best write throughput |
| MATLAB users | `hdf5` | Widest cross-platform support |
| Self-describing archive | `hdf5` | Rich metadata, hierarchical |
| Climate/science | `netcdf` | CF conventions, metadata |
| SQL analytics | `duckdb` | In-process SQL on seismic data |

### Not Recommended for Production

| Format | Issue |
|--------|-------|
| `json` | 4× size overhead, 100× slower read than binary |
| `tensorstore` (hot path) | Python subprocess overhead |
| `mdio` (hot path) | Python subprocess + xarray overhead, 2 MB/s |
| `duckdb` (bulk array) | Row-based insert, not designed for dense arrays |

## Implementation Details

### Native C++ Formats
- **binary_f32 / binary_header / mmap**: Zero-dependency, maximum performance
- **npy**: Via cnpy header-only library
- **json**: Via nlohmann/json header-only library
- **segy**: Native C++ SEG-Y rev1 writer/reader with EBCDIC + binary headers

### C Library Bindings
- **hdf5**: HighFive wrapper + C API for chunked I/O
- **netcdf**: libnetcdf C API
- **tiledb**: libtiledb C API for dense arrays
- **duckdb**: libduckdb C API for SQL queries
- **parquet**: Apache Arrow C++ API, long-format columnar storage

### Python Bridge Formats
- **zarr**: Direct zarr v2 write/read via Python subprocess
- **tensorstore**: Python tensorstore → zarr driver
- **mdio**: mdio.to_mdio / open_mdio via Python subprocess (requires `multidimio` package)
- **miniseed**: obspy Trace → miniSEED via Python subprocess (requires `obspy` package)

> **Note**: Python bridge formats have inherent subprocess overhead (~500ms startup).
> Their throughput numbers reflect this overhead and are not directly comparable to
> native C++ formats. They are included for format coverage and cloud workflow relevance.

## Dependencies

| Library | Version | Formats | Install |
|---------|---------|---------|---------|
| nlohmann/json | 3.x | json | Bundled (git submodule) |
| cnpy | latest | npy | Bundled (git submodule) |
| HighFive | — | hdf5 | linuxbrew |
| libnetcdf | 22 | netcdf | linuxbrew |
| libtiledb | 2.30 | tiledb | linuxbrew |
| libduckdb | 1.5.1 | duckdb | linuxbrew |
| Apache Arrow | 23.0.1 | parquet | linuxbrew |
| Python tensorstore | 0.1.82 | tensorstore | pip |
| Python multidimio | 1.1.2 | mdio | pip (requires Python 3.11-3.13) |
| Python obspy | 1.5.0 | miniseed | pip |

## Adoption Roadmap

### Phase 1: Core Formats ✅ COMPLETE
- [x] binary_f32, binary_header, mmap, npy, json

### Phase 2: Scientific Formats ✅ COMPLETE
- [x] hdf5, netcdf, segy (native C++)

### Phase 3: Cloud/HPC Formats ✅ COMPLETE
- [x] zarr, tiledb, parquet, duckdb, tensorstore

### Phase 4: Geophysics-Native Formats ✅ COMPLETE
- [x] mdio (cloud-native seismic via Python bridge)
- [x] miniseed (seismological time series via obspy)

### Phase 5: Geophysics Presets ✅ COMPLETE
- [x] 6 built-in presets: 2d-survey-line, 2d-velocity-model, 3d-velocity-model, 3d-large-survey, shot-gather, checkpoint-restart

### Future Considerations
- [ ] OpenVDS (Equinor cloud-optimized seismic, C++ native)
- [ ] ASDF (Adaptable Seismic Data Format, HDF5-based)
- [ ] RSF (Madagascar Regularly Sampled Format)
- [ ] Native C++ libmseed adapter (replace Python bridge)
- [ ] ADIOS2 BP format (if library becomes available)
- [ ] Streaming/append benchmark mode

## Conclusions

1. **Binary formats are essential** for production RTM hot paths — JSON is impractical for large arrays
2. **HDF5 is the best general-purpose format** for seismic: 4 GB/s read, rich metadata, wide tool support
3. **SEG-Y remains viable** at scale: 120-159 MB/s, universal tool compatibility
4. **MiniSEED fills the seismology niche**: trace-based, real-time streaming, 17 MB/s via Python
5. **MDIO is cloud-native but slow**: 2 MB/s via Python bridge — suitable only for offline cloud workflows
6. **C++ offers 5-80× improvement** over Python for native format throughput
7. **Parquet strong for analytics**: 484 MB/s read with columnar structure, good for ML pipelines
8. **Geophysics presets enable targeted benchmarking** for realistic seismic workloads

## References

- Python benchmark: `rtm3d-cli/scripts/io_format_benchmark.py`
- SEG-Y format: https://library.seg.org/pb-assets/technical-standards/seg_y_rev1.pdf
- MDIO: https://github.com/TGSAI/mdio-python
- MiniSEED / libmseed: https://earthscope.github.io/libmseed/
- NPY format: https://numpy.org/doc/stable/reference/generated/numpy.lib.format.html
- HDF5: https://www.hdfgroup.org/solutions/hdf5/
- Apache Arrow/Parquet: https://arrow.apache.org/
- obspy: https://docs.obspy.org/
