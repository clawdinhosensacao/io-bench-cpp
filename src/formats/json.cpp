#include "io_bench/formats.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace io_bench {

using json = nlohmann::json;

void JsonFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // Write as 2D array [[row0], [row1], ...]
    json j = json::array();
    for (std::size_t z = 0; z < shape.nz; ++z) {
        json row = json::array();
        for (std::size_t x = 0; x < shape.nx; ++x) {
            row.push_back(data[z * shape.nx + x]);
        }
        j.push_back(std::move(row));
    }
    
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    file << j.dump();
}

void JsonFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open file for reading: " + path);
    }
    
    json j;
    file >> j;
    
    if (!j.is_array() || j.size() != shape.nz) {
        throw std::runtime_error("Invalid JSON array shape");
    }
    
    for (std::size_t z = 0; z < shape.nz; ++z) {
        const auto& row = j[z];
        if (!row.is_array() || row.size() != shape.nx) {
            throw std::runtime_error("Invalid JSON row shape");
        }
        for (std::size_t x = 0; x < shape.nx; ++x) {
            data[z * shape.nx + x] = row[x].get<float>();
        }
    }
}

} // namespace io_bench
