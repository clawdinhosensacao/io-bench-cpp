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
    std::vector<size_t> dims;
    if (shape.is_3d()) {
        dims = {shape.ny, shape.nz, shape.nx};
    } else {
        dims = {shape.nz, shape.nx};
    }
    cnpy::npy_save(path, data, dims, "w");  // "w" = write mode (overwrite)
}

void NpyFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    cnpy::NpyArray arr = cnpy::npy_load(path);
    
    // Verify shape
    bool shape_ok = false;
    if (shape.is_3d() && arr.shape.size() == 3) {
        shape_ok = (arr.shape[0] == shape.ny && 
                    arr.shape[1] == shape.nz && 
                    arr.shape[2] == shape.nx);
    } else if (!shape.is_3d() && arr.shape.size() == 2) {
        shape_ok = (arr.shape[0] == shape.nz && arr.shape[1] == shape.nx);
    }
    
    if (!shape_ok) {
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
