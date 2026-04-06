#include "io_bench/formats.hpp"
#include <memory>

namespace io_bench {
namespace formats {

std::vector<std::unique_ptr<FormatAdapter>> get_all_adapters() {
    std::vector<std::unique_ptr<FormatAdapter>> adapters;
    
    // Core formats (always available)
    adapters.push_back(std::make_unique<BinaryFormat>());
    adapters.push_back(std::make_unique<BinaryHeaderFormat>());
    adapters.push_back(std::make_unique<NpyFormat>());
    adapters.push_back(std::make_unique<JsonFormat>());
    adapters.push_back(std::make_unique<MmapFormat>());
    adapters.push_back(std::make_unique<ZarrFormat>());
    adapters.push_back(std::make_unique<SegyFormat>());
    
    // Optional formats
#ifdef HAVE_HDF5
    adapters.push_back(std::make_unique<Hdf5Format>());
#endif

#ifdef HAVE_NETCDF
    adapters.push_back(std::make_unique<NetcdfFormat>());
#endif

#ifdef HAVE_TILEDB
    adapters.push_back(std::make_unique<TileDBFormat>());
#endif

#ifdef HAVE_ADIOS2
    adapters.push_back(std::make_unique<Adios2Format>());
#endif

#ifdef HAVE_DUCKDB
    adapters.push_back(std::make_unique<DuckDBFormat>());
#endif
    
    return adapters;
}

}  // namespace formats
}  // namespace io_bench
