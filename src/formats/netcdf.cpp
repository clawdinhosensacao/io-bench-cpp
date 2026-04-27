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
    int ncid;
    if (nc_create(path.c_str(), NC_CLOBBER, &ncid) != NC_NOERR) {
        throw std::runtime_error("Failed to create NetCDF file: " + path);
    }
    
    int z_dimid;
    int x_dimid;
    int y_dimid;
    nc_def_dim(ncid, "z", shape.nz, &z_dimid);
    nc_def_dim(ncid, "x", shape.nx, &x_dimid);
    
    int varid;
    if (shape.is_3d()) {
        nc_def_dim(ncid, "y", shape.ny, &y_dimid);
        int dimids[3] = {y_dimid, z_dimid, x_dimid};
        nc_def_var(ncid, "velocity", NC_FLOAT, 3, dimids, &varid);
    } else {
        int dimids[2] = {z_dimid, x_dimid};
        nc_def_var(ncid, "velocity", NC_FLOAT, 2, dimids, &varid);
    }
    
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
    int ncid;
    int varid;
    
    if (nc_open(path.c_str(), NC_NOWRITE, &ncid) != NC_NOERR) {
        throw std::runtime_error("Failed to open NetCDF file: " + path);
    }
    
    nc_inq_varid(ncid, "velocity", &varid);
    
    int ndims;
    nc_inq_varndims(ncid, varid, &ndims);
    
    int dimids[3];
    nc_inq_vardimid(ncid, varid, dimids);
    
    size_t z_len = 0;
    size_t x_len = 0;
    size_t y_len = 1;
    nc_inq_dimlen(ncid, dimids[ndims - 2], &z_len);
    nc_inq_dimlen(ncid, dimids[ndims - 1], &x_len);
    if (ndims == 3) {
        nc_inq_dimlen(ncid, dimids[0], &y_len);
    }
    
    if (z_len != shape.nz || x_len != shape.nx || y_len != shape.ny) {
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

void NetcdfFormat::read_slice(const std::string& path, float* slice_buf,
                                const ArrayShape& shape, std::size_t iy) {
#ifdef HAVE_NETCDF
    int ncid;
    int varid;

    if (nc_open(path.c_str(), NC_NOWRITE, &ncid) != NC_NOERR) {
        throw std::runtime_error("Failed to open NetCDF file: " + path);
    }

    nc_inq_varid(ncid, "velocity", &varid);

    // NetCDF variable layout: (y, z, x) for 3D
    // Read one inline: start at [iy, 0, 0], count [1, nz, nx]
    size_t start[3] = {iy, 0, 0};
    size_t count[3] = {1, shape.nz, shape.nx};

    int err = nc_get_vara_float(ncid, varid, start, count, slice_buf);
    nc_close(ncid);

    if (err != NC_NOERR) {
        throw std::runtime_error("NetCDF slice read failed");
    }
#else
    (void)path;
    (void)slice_buf;
    (void)shape;
    (void)iy;
    throw std::runtime_error("NetCDF support not compiled in");
#endif
}

void NetcdfFormat::write_compressed(const std::string& path, const float* data,
                                      const ArrayShape& shape, int level) {
#ifdef HAVE_NETCDF
    int ncid;
    if (nc_create(path.c_str(), NC_CLOBBER | NC_NETCDF4, &ncid) != NC_NOERR) {
        throw std::runtime_error("Failed to create NetCDF4 file: " + path);
    }

    int z_dimid;
    int x_dimid;
    int y_dimid;
    nc_def_dim(ncid, "z", shape.nz, &z_dimid);
    nc_def_dim(ncid, "x", shape.nx, &x_dimid);

    int varid;
    if (shape.is_3d()) {
        nc_def_dim(ncid, "y", shape.ny, &y_dimid);
        int dimids[3] = {y_dimid, z_dimid, x_dimid};
        nc_def_var(ncid, "velocity", NC_FLOAT, 3, dimids, &varid);
    } else {
        int dimids[2] = {z_dimid, x_dimid};
        nc_def_var(ncid, "velocity", NC_FLOAT, 2, dimids, &varid);
    }

    // Enable chunking for compression (required for deflate)
    if (shape.is_3d()) {
        size_t chunksize[3] = {
            std::min(static_cast<size_t>(shape.ny), static_cast<size_t>(64)),
            std::min(static_cast<size_t>(shape.nz), static_cast<size_t>(64)),
            std::min(static_cast<size_t>(shape.nx), static_cast<size_t>(64))};
        nc_def_var_chunking(ncid, varid, NC_CHUNKED, chunksize);
    } else {
        size_t chunksize[2] = {
            std::min(static_cast<size_t>(shape.nz), static_cast<size_t>(64)),
            std::min(static_cast<size_t>(shape.nx), static_cast<size_t>(64))};
        nc_def_var_chunking(ncid, varid, NC_CHUNKED, chunksize);
    }

    // Apply zlib compression for levels 1-9; level 0 = no compression
    if (level > 0) {
        nc_def_var_deflate(ncid, varid, 0, 1, static_cast<int>(level));
    }

    nc_enddef(ncid);
    nc_put_var_float(ncid, varid, data);
    nc_close(ncid);
#else
    (void)path;
    (void)data;
    (void)shape;
    (void)level;
    throw std::runtime_error("NetCDF support not compiled in");
#endif
}

} // namespace io_bench
