#include "io_bench/formats.hpp"

#ifdef HAVE_HDF5
#include <hdf5.h>
#include <cstring>
#endif

namespace io_bench {

bool Hdf5Format::is_available() const {
#ifdef HAVE_HDF5
    return true;
#else
    return false;
#endif
}

void Hdf5Format::write(const std::string& path, const float* data, const ArrayShape& shape) {
#ifdef HAVE_HDF5
    hid_t file = H5Fcreate(path.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file < 0) {
        throw std::runtime_error("Failed to create HDF5 file: " + path);
    }
    
    hid_t dataspace;
    if (shape.is_3d()) {
        hsize_t dims[3] = {shape.ny, shape.nz, shape.nx};
        dataspace = H5Screate_simple(3, dims, nullptr);
    } else {
        hsize_t dims[2] = {shape.nz, shape.nx};
        dataspace = H5Screate_simple(2, dims, nullptr);
    }
    
    hid_t dataset = H5Dcreate2(file, "/velocity", H5T_NATIVE_FLOAT, dataspace,
                                H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    
    H5Dwrite(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);
    
    H5Dclose(dataset);
    H5Sclose(dataspace);
    H5Fclose(file);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("HDF5 support not compiled in");
#endif
}

void Hdf5Format::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_HDF5
    hid_t file = H5Fopen(path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file < 0) {
        throw std::runtime_error("Failed to open HDF5 file: " + path);
    }
    
    hid_t dataset = H5Dopen2(file, "/velocity", H5P_DEFAULT);
    if (dataset < 0) {
        H5Fclose(file);
        throw std::runtime_error("Failed to open /velocity dataset");
    }
    
    hid_t dataspace = H5Dget_space(dataset);
    int ndims = H5Sget_simple_extent_ndims(dataspace);
    hsize_t dims[3];
    H5Sget_simple_extent_dims(dataspace, dims, nullptr);
    
    bool shape_ok = false;
    if (shape.is_3d() && ndims == 3) {
        shape_ok = (dims[0] == shape.ny && dims[1] == shape.nz && dims[2] == shape.nx);
    } else if (!shape.is_3d() && ndims == 2) {
        shape_ok = (dims[0] == shape.nz && dims[1] == shape.nx);
    }
    
    if (!shape_ok) {
        H5Sclose(dataspace);
        H5Dclose(dataset);
        H5Fclose(file);
        throw std::runtime_error("HDF5 dataset shape mismatch");
    }
    
    H5Dread(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);
    
    H5Sclose(dataspace);
    H5Dclose(dataset);
    H5Fclose(file);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("HDF5 support not compiled in");
#endif
}

void Hdf5Format::read_slice(const std::string& path, float* slice_buf,
                              const ArrayShape& shape, std::size_t iy) {
#ifdef HAVE_HDF5
    hid_t file = H5Fopen(path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file < 0) {
        throw std::runtime_error("Failed to open HDF5 file: " + path);
    }

    hid_t dataset = H5Dopen2(file, "/velocity", H5P_DEFAULT);
    if (dataset < 0) {
        H5Fclose(file);
        throw std::runtime_error("Failed to open /velocity dataset");
    }

    hid_t file_space = H5Dget_space(dataset);

    // Define hyperslab selection: select one inline (iy) from the 3D dataset
    // Dataset layout: (ny, nz, nx) — select iy-th inline
    hsize_t file_start[3] = {static_cast<hsize_t>(iy), 0, 0};
    hsize_t file_count[3] = {1, static_cast<hsize_t>(shape.nz), static_cast<hsize_t>(shape.nx)};
    H5Sselect_hyperslab(file_space, H5S_SELECT_SET, file_start, nullptr, file_count, nullptr);

    // Memory space: 2D array of (nz, nx)
    hsize_t mem_dims[2] = {static_cast<hsize_t>(shape.nz), static_cast<hsize_t>(shape.nx)};
    hid_t mem_space = H5Screate_simple(2, mem_dims, nullptr);

    H5Dread(dataset, H5T_NATIVE_FLOAT, mem_space, file_space, H5P_DEFAULT, slice_buf);

    H5Sclose(mem_space);
    H5Sclose(file_space);
    H5Dclose(dataset);
    H5Fclose(file);
#else
    (void)path;
    (void)slice_buf;
    (void)shape;
    (void)iy;
    throw std::runtime_error("HDF5 support not compiled in");
#endif
}

} // namespace io_bench
