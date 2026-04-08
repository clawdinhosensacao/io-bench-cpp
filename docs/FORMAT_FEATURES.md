# I/O Format Feature Matrix

This document compares key I/O features across all benchmarked formats,
focusing on capabilities relevant to **geophysics and seismic processing** workflows.

## Feature Definitions

| Feature | Description | Geophysics Relevance |
|---------|-------------|---------------------|
| **Chunking** | Data stored in independent blocks; enables partial reads | Extract inline/crossline slices from 3D volumes without loading entire file |
| **Compression** | Lossless or lossy compression to reduce storage | Seismic surveys are TB-scale; compression saves cost |
| **Parallel I/O** | Multi-threaded or multi-process concurrent read/write | RTM checkpoints, distributed processing |
| **Direct Access** | Memory-mapped or O_DIRECT bypass of page cache | Hot paths in RTM propagation loops |
| **Metadata** | Self-describing headers with dimensional info | Prevents data loss from detached headers |
| **Streaming/Append** | Append-only writes for real-time acquisition | Field recording, real-time monitoring |
| **Cloud/Native** | Native cloud object store access (S3, GCS, Azure) | Cloud-native seismic processing |
| **3D+ Support** | Native multi-dimensional arrays (3D, 4D, 5D) | Velocity models, shot gathers, 4D time-lapse |
| **Partial Read** | Read subset without loading entire file | Inline/crossline extraction, trace access |
| **Interoperability** | Cross-language, cross-tool support | Python/C++/Julia workflows, processing pipelines |

## Format Feature Matrix

| Format | Chunking | Compression | Parallel I/O | Direct Access | Metadata | Streaming | Cloud | 3D+ | Partial Read | Interop |
|--------|----------|-------------|--------------|---------------|----------|-----------|-------|------|--------------|---------|
| binary_f32 | ❌ | ❌ | ❌ | ✅ mmap | ❌ | ❌ | ❌ | ❌ | ❌ | Low |
| binary_header | ❌ | ❌ | ❌ | ✅ mmap | ✅ minimal | ❌ | ❌ | ❌ | ❌ | Low |
| mmap | ❌ | ❌ | ✅ OS-level | ✅ native | ❌ | ❌ | ❌ | ❌ | ✅ offset | Low |
| rsf | ❌ | ❌ | ❌ | ✅ mmap | ✅ text header | ❌ | ❌ | ✅ n1-n6 | ✅ offset | Madagascar |
| npy | ❌ | ❌ | ❌ | ✅ mmap | ✅ header | ❌ | ❌ | ✅ shape | ✅ offset | NumPy |
| json | ❌ | ❌ | ❌ | ❌ | ✅ self-describing | ❌ | ❌ | ❌ | ❌ | Universal |
| hdf5 | ✅ chunks | ✅ gzip/szip/zstd | ✅ MPI-IO | ✅ mmap | ✅ rich | ✅ append | ✅ VFD/S3 | ✅ unlimited | ✅ Wide |
| netcdf | ✅ chunks | ✅ zlib/zstd | ✅ pnetcdf | ✅ mmap | ✅ CF conventions | ✅ append | ✅ S3 | ✅ unlimited | ✅ slicing | ✅ Wide |
| segy | ❌ trace-based | ❌ | ❌ | ✅ mmap | ✅ trace headers | ✅ append | ❌ | ❌ 2D traces | ✅ trace # | ✅ Industry std |
| segd | ❌ record-based | ❌ | ❌ | ✅ mmap | ✅ record headers | ✅ append | ❌ | ❌ | ✅ record # | ✅ Acquisition |
| parquet | ✅ row groups | ✅ snappy/gzip/zstd | ✅ multi-thread | ❌ | ✅ schema | ✅ append | ✅ S3/Azure | ❌ flat table | ✅ row groups | ✅ Wide |
| tiledb | ✅ tiles | ✅ zlib/zstd/bzip2 | ✅ multi-thread | ✅ mmap | ✅ rich | ✅ append | ✅ S3 | ✅ ND arrays | ✅ slicing | Python/R/C++ |
| zarr | ✅ chunks | ✅ blosc/zlib/gzip | ✅ per-chunk | ❌ | ✅ .zarray JSON | ✅ append | ✅ S3/GCS/Azure | ✅ ND arrays | ✅ slicing | Python/C++/Julia |
| mdio | ✅ chunks | ✅ blosc/zfp | ✅ dask | ❌ | ✅ seismic-specific | ✅ append | ✅ S3/GCS/Azure | ✅ ND arrays | ✅ slicing | Python/xarray |
| miniseed | ❌ record-based | ✅ Steim-1/2 | ❌ | ✅ mmap | ✅ FDSN headers | ✅ real-time | ❌ | ❌ time-series | ✅ time window | ✅ Seismology std |
| asdf | ✅ blocks | ✅ gzip/bzip2/lz4 | ❌ | ❌ | ✅ rich (HDF5) | ✅ append | ❌ | ❌ 1D traces | ✅ time window | ✅ Seismology |
| tensorstore | ✅ chunks | ✅ zlib/gzip/zstd | ✅ multi-thread | ❌ | ✅ schema | ❌ | ✅ S3/GCS/Azure | ✅ ND arrays | ✅ slicing | Python/C++ |

## Key Findings for Geophysics

### Tier 1: Production-Ready for Seismic Workflows
- **HDF5**: Full feature set (chunking, compression, parallel, metadata, 3D+). Gold standard for self-describing scientific data.
- **Zarr**: Cloud-native, chunked, compressed. Emerging standard for large-scale geophysics.
- **MDIO**: Purpose-built for seismic data. Zarr-based with seismic-specific chunking templates.

### Tier 2: Domain-Specific Standards
- **SEG-Y**: Industry standard for trace data. No chunking but trace-based partial access.
- **MiniSEED**: Real-time seismology standard. Steim compression for time-series.
- **RSF**: Open-source geophysics (Madagascar). Simple, fast, but no chunking/compression.

### Tier 3: General-Purpose High-Performance
- **TileDB**: Full feature set including cloud, compression, multi-threaded. Strong for array data.
- **Parquet**: Columnar, compressed, cloud-native. Good for tabular seismic metadata.
- **TensorStore**: C++/Python API, cloud-native, chunked, compressed, multi-threaded. Designed for large multi-dimensional arrays.

### Tier 4: Simple/Reference Baselines
- **binary/mmap/npy**: Maximum throughput baseline. No chunking or compression.
- **JSON**: Universal but extremely slow. Metadata-only use case.

## Recommendations

### For RTM/Imaging Workflows
1. **Checkpoint/Restart**: Use HDF5 or Zarr (chunked + compressed + 3D)
2. **Velocity Model I/O**: Use RSF (fast) or HDF5 (self-describing)
3. **Large 3D Volumes**: Use Zarr or MDIO (cloud-native + chunked)

### For Seismic Acquisition
1. **Field Recording**: SEG-D (acquisition), SEG-Y (exchange)
2. **Real-Time Monitoring**: MiniSEED (Steim compression, streaming)

### For Cloud-Native Processing
1. **Primary**: Zarr or MDIO (chunked, compressed, S3-native)
2. **High-Performance**: TensorStore C++ API (multi-threaded, direct chunk access)

### For Big Data / Parallel Processing
1. **HDF5 + MPI-IO**: Traditional HPC parallel I/O
2. **TileDB**: Multi-threaded array I/O with cloud support
3. **TensorStore C++ API**: Parallel chunk reads, designed for TB-scale data

## Future Features to Implement in io-bench-cpp

### High Priority (Geophysics Impact)
1. **Chunked Read Benchmark**: Read single inline from 3D volume (measures chunking efficiency)
2. **Parallel I/O Benchmark**: Multi-threaded reads of same volume (measures concurrency scaling)
3. **Big Volume Benchmark**: 401×401×201 (~300MB) with throughput tracking
4. **Compression Ratio Comparison**: Same data, measure size reduction per format

### Medium Priority (Feature Coverage)
5. **Streaming Write Benchmark**: Append traces one at a time (acquisition simulation)
6. **Direct I/O (O_DIRECT)**: Bypass page cache to measure raw disk throughput
7. **Cloud I/O Benchmark**: S3/GCS read performance (mock or real)

### Low Priority (Nice-to-Have)
8. **Compression Level Sweep**: Same format, different compression levels
9. **Memory Usage Tracking**: Peak RSS during read/write operations
10. **Random Access Pattern**: Non-sequential reads (trace picking, horizon extraction)
