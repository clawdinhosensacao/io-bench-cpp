#include "io_bench/formats.hpp"

#ifdef HAVE_HDF5
#include <highfive/H5File.hpp>
#include <highfive/H5DataSet.hpp>
#include <highfive/H5DataSpace.hpp>
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
    HighFive::File file(path, HighFive::File::Overwrite);
    std::vector<size_t> dims = {shape.nz, shape.nx};
    HighFive::DataSpace dataspace(dims);
    auto dataset = file.createDataSet<float>("/velocity", dataspace);
    dataset.write_raw(data);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("HDF5 support not compiled in");
#endif
}

void Hdf5Format::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_HDF5
    HighFive::File file(path, HighFive::File::ReadOnly);
    auto dataset = file.getDataSet("/velocity");
    auto dims = dataset.getSpace().getDimensions();
    
    if (dims.size() != 2 || dims[0] != shape.nz || dims[1] != shape.nx) {
        throw std::runtime_error("HDF5 dataset shape mismatch");
    }
    
    dataset.read_raw(data);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("HDF5 support not compiled in");
#endif
}

} // namespace io_bench
