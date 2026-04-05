#include "io_bench/formats.hpp"
#include "cnpy.h"
#include <cstring>
#include <stdexcept>
#include <vector>

namespace io_bench {

bool NpyFormat::is_available() const {
    return true;  // cnpy is always available via FetchContent
}

void NpyFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // cnpy expects std::vector for shape
    std::vector<size_t> dims = {shape.nz, shape.nx};
    cnpy::npy_save(path, data, dims, "w");  // "w" = write mode (overwrite)
}

void NpyFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    cnpy::NpyArray arr = cnpy::npy_load(path);
    
    // Verify shape
    if (arr.shape.size() != 2 || 
        arr.shape[0] != shape.nz || 
        arr.shape[1] != shape.nx) {
        throw std::runtime_error("NPY shape mismatch");
    }
    
    // Verify type
    if (arr.word_size != sizeof(float)) {
        throw std::runtime_error("NPY type mismatch: expected float32");
    }
    
    // Copy data
    std::memcpy(data, arr.data<float>(), shape.bytes());
}

} // namespace io_bench
