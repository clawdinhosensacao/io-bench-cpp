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
    
    hsize_t dims[2] = {shape.nz, shape.nx};
    hid_t dataspace = H5Screate_simple(2, dims, nullptr);
    
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
    hsize_t dims[2];
    H5Sget_simple_extent_dims(dataspace, dims, nullptr);
    
    if (dims[0] != shape.nz || dims[1] != shape.nx) {
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

} // namespace io_bench
