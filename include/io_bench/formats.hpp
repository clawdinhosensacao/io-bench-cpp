#pragma once

#include "io_bench/types.hpp"

namespace io_bench {

/// Raw binary float32 format (no header)
class BinaryFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "binary_f32"; }
    [[nodiscard]] bool is_available() const override { return true; }
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".bin"; }
};

/// Binary with header (nx, nz as uint64)
class BinaryHeaderFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "binary_header"; }
    [[nodiscard]] bool is_available() const override { return true; }
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".binh"; }
};

/// NumPy NPY format
class NpyFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "npy"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".npy"; }
};

/// JSON array format
class JsonFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "json"; }
    [[nodiscard]] bool is_available() const override { return true; }
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".json"; }
};

/// HDF5 format (optional)
class Hdf5Format : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "hdf5"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".h5"; }
};

/// NetCDF4 format (optional)
class NetcdfFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "netcdf"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".nc"; }
};

/// TileDB format (optional)
class TileDBFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "tiledb"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".tiledb"; }
};

/// ADIOS2 BP format (optional)
class Adios2Format : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "adios2"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".bp"; }
};

/// Memory-mapped binary (for comparison)
class MmapFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "mmap"; }
    [[nodiscard]] bool is_available() const override { return true; }
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".mmap"; }
};

/// Zarr format (placeholder)
class ZarrFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "zarr"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".zarr"; }
};

/// Parquet format (placeholder)
class ParquetFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "parquet"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".parquet"; }
};

/// SEG-Y format (placeholder)
class SegyFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "segy"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".segy"; }
};

/// TensorStore format (placeholder)
class TensorStoreFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "tensorstore"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".tstore"; }
};

/// MDIO format (placeholder)
class MdioFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "mdio"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".mdio"; }
};

/// DuckDB format (SQL database)
class DuckDBFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "duckdb"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".duckdb"; }
};

/// MiniSEED format (seismological time series via libmseed C API)
class MiniSeedFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "miniseed"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".mseed"; }
};

/// RSF format (Madagascar Regularly Sampled Format — native C++)
class RsfFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "rsf"; }
    [[nodiscard]] bool is_available() const override { return true; }
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".rsf"; }
};

/// ASDF format (Adaptable Seismic Data Format — HDF5-based, via pyasdf)
class AsdfFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "asdf"; }
    [[nodiscard]] bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".h5"; }
};

/// SEGD format (SEG-D field recording format — native C++)
class SegDFormat : public FormatAdapter {
public:
    [[nodiscard]] std::string name() const override { return "segd"; }
    [[nodiscard]] bool is_available() const override { return true; }
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    [[nodiscard]] std::string extension() const override { return ".sgd"; }
};

} // namespace io_bench
