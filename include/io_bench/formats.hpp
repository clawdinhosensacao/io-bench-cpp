#pragma once

#include "io_bench/types.hpp"

namespace io_bench {

/// Raw binary float32 format (no header)
class BinaryFormat : public FormatAdapter {
public:
    std::string name() const override { return "binary_f32"; }
    bool is_available() const override { return true; }
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    std::string extension() const override { return ".bin"; }
};

/// Binary with header (nx, nz as uint64)
class BinaryHeaderFormat : public FormatAdapter {
public:
    std::string name() const override { return "binary_header"; }
    bool is_available() const override { return true; }
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    std::string extension() const override { return ".binh"; }
};

/// NumPy NPY format
class NpyFormat : public FormatAdapter {
public:
    std::string name() const override { return "npy"; }
    bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    std::string extension() const override { return ".npy"; }
};

/// JSON array format
class JsonFormat : public FormatAdapter {
public:
    std::string name() const override { return "json"; }
    bool is_available() const override { return true; }
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    std::string extension() const override { return ".json"; }
};

/// HDF5 format (optional)
class Hdf5Format : public FormatAdapter {
public:
    std::string name() const override { return "hdf5"; }
    bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    std::string extension() const override { return ".h5"; }
};

/// NetCDF4 format (optional)
class NetcdfFormat : public FormatAdapter {
public:
    std::string name() const override { return "netcdf"; }
    bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    std::string extension() const override { return ".nc"; }
};

/// TileDB format (optional)
class TileDBFormat : public FormatAdapter {
public:
    std::string name() const override { return "tiledb"; }
    bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    std::string extension() const override { return ".tiledb"; }
};

/// ADIOS2 BP format (optional)
class Adios2Format : public FormatAdapter {
public:
    std::string name() const override { return "adios2"; }
    bool is_available() const override;
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    std::string extension() const override { return ".bp"; }
};

/// Memory-mapped binary (for comparison)
class MmapFormat : public FormatAdapter {
public:
    std::string name() const override { return "mmap"; }
    bool is_available() const override { return true; }
    void write(const std::string& path, const float* data, const ArrayShape& shape) override;
    void read(const std::string& path, float* data, const ArrayShape& shape) override;
    std::string extension() const override { return ".mmap"; }
};

} // namespace io_bench
