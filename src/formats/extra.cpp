#include "io_bench/formats.hpp"
#include <stdexcept>

namespace io_bench {

// Zarr, Segy, DuckDB, Parquet, and TensorStore are now in dedicated files

// MDIO format - Seismic MDIO (no C++ library available)
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
