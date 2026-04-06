#include "io_bench/formats.hpp"
#include <stdexcept>

namespace io_bench {

// Zarr, Segy, and DuckDB are now in dedicated files

// Parquet format - needs Arrow Parquet C++ API
bool ParquetFormat::is_available() const {
#ifdef HAVE_PARQUET
    return true;
#else
    return false;
#endif
}

void ParquetFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
#ifdef HAVE_PARQUET
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("Parquet write not implemented");
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("Parquet support not compiled in");
#endif
}

void ParquetFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_PARQUET
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("Parquet read not implemented");
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("Parquet support not compiled in");
#endif
}

// TensorStore format - Google's tensor storage
bool TensorStoreFormat::is_available() const {
#ifdef HAVE_TENSORSTORE
    return true;
#else
    return false;
#endif
}

void TensorStoreFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
#ifdef HAVE_TENSORSTORE
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("TensorStore write not implemented");
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("TensorStore support not compiled in");
#endif
}

void TensorStoreFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_TENSORSTORE
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("TensorStore read not implemented");
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("TensorStore support not compiled in");
#endif
}

// MDIO format - Seismic MDIO
bool MdioFormat::is_available() const {
#ifdef HAVE_MDIO
    return true;
#else
    return false;
#endif
}

void MdioFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
#ifdef HAVE_MDIO
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("MDIO write not implemented");
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("MDIO support not compiled in");
#endif
}

void MdioFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_MDIO
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("MDIO read not implemented");
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("MDIO support not compiled in");
#endif
}

} // namespace io_bench
