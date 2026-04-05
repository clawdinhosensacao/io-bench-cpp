#include "io_bench/formats.hpp"
#include <stdexcept>

#ifdef HAVE_NETCDF
#include <netcdf.h>
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
    int ncid, z_dimid, x_dimid, varid;
    int dimids[2];
    
    if (nc_create(path.c_str(), NC_CLOBBER, &ncid) != NC_NOERR) {
        throw std::runtime_error("Failed to create NetCDF file: " + path);
    }
    
    nc_def_dim(ncid, "z", shape.nz, &z_dimid);
    nc_def_dim(ncid, "x", shape.nx, &x_dimid);
    
    dimids[0] = z_dimid;
    dimids[1] = x_dimid;
    nc_def_var(ncid, "velocity", NC_FLOAT, 2, dimids, &varid);
    nc_enddef(ncid);
    
    nc_put_var_float(ncid, varid, data);
    nc_close(ncid);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("NetCDF support not compiled in");
#endif
}

void NetcdfFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_NETCDF
    int ncid, varid;
    
    if (nc_open(path.c_str(), NC_NOWRITE, &ncid) != NC_NOERR) {
        throw std::runtime_error("Failed to open NetCDF file: " + path);
    }
    
    nc_inq_varid(ncid, "velocity", &varid);
    
    size_t z_len, x_len;
    int dimids[2];
    nc_inq_vardimid(ncid, varid, dimids);
    nc_inq_dimlen(ncid, dimids[0], &z_len);
    nc_inq_dimlen(ncid, dimids[1], &x_len);
    
    if (z_len != shape.nz || x_len != shape.nx) {
        nc_close(ncid);
        throw std::runtime_error("NetCDF variable shape mismatch");
    }
    
    nc_get_var_float(ncid, varid, data);
    nc_close(ncid);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("NetCDF support not compiled in");
#endif
}

} // namespace io_bench
