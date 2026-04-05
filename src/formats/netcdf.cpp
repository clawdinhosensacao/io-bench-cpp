#include "io_bench/formats.hpp"
#include <stdexcept>

#ifdef HAVE_NETCDF
#include <netcdf>
using namespace netCDF;
#endif

namespace io_bench {

bool NetcdfFormat::is_available() const {
#ifdef HAVE_NETCDF
    return true;
#else
    return false;
#endif
}

void NetcdfFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
#ifdef HAVE_NETCDF
    NcFile file(path, NcFile::replace);
    
    // Define dimensions
    NcDim zDim = file.addDim("z", shape.nz);
    NcDim xDim = file.addDim("x", shape.nx);
    
    // Define variable
    std::vector<NcDim> dims = {zDim, xDim};
    NcVar var = file.addVar("velocity", ncFloat, dims);
    
    // Write data
    var.putVar(data);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("NetCDF support not compiled in");
#endif
}

void NetcdfFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_NETCDF
    NcFile file(path, NcFile::read);
    NcVar var = file.getVar("velocity");
    
    std::vector<NcDim> dims = var.getDims();
    if (dims.size() != 2 || dims[0].getSize() != shape.nz || dims[1].getSize() != shape.nx) {
        throw std::runtime_error("NetCDF variable shape mismatch");
    }
    
    var.getVar(data);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("NetCDF support not compiled in");
#endif
}

} // namespace io_bench
