#include "io_bench/formats.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace io_bench {

using json = nlohmann::json;

void JsonFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // Write as 2D or 3D array
    json j;
    
    if (shape.is_3d()) {
        // 3D: [[[plane0], [plane1], ...], ...]
        j = json::array();
        for (std::size_t y = 0; y < shape.ny; ++y) {
            json plane = json::array();
            for (std::size_t z = 0; z < shape.nz; ++z) {
                json row = json::array();
                for (std::size_t x = 0; x < shape.nx; ++x) {
                    std::size_t idx = y * shape.nz * shape.nx + z * shape.nx + x;
                    row.push_back(data[idx]);
                }
                plane.push_back(std::move(row));
            }
            j.push_back(std::move(plane));
        }
    } else {
        // 2D: [[row0], [row1], ...]
        j = json::array();
        for (std::size_t z = 0; z < shape.nz; ++z) {
            json row = json::array();
            for (std::size_t x = 0; x < shape.nx; ++x) {
                row.push_back(data[z * shape.nx + x]);
            }
            j.push_back(std::move(row));
        }
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
    
    if (!j.is_array()) {
        throw std::runtime_error("Invalid JSON: expected array");
    }
    
    if (shape.is_3d()) {
        // 3D
        if (j.size() != shape.ny) {
            throw std::runtime_error("Invalid JSON 3D array shape (ny)");
        }
        for (std::size_t y = 0; y < shape.ny; ++y) {
            const auto& plane = j[y];
            if (!plane.is_array() || plane.size() != shape.nz) {
                throw std::runtime_error("Invalid JSON 3D array shape (nz)");
            }
            for (std::size_t z = 0; z < shape.nz; ++z) {
                const auto& row = plane[z];
                if (!row.is_array() || row.size() != shape.nx) {
                    throw std::runtime_error("Invalid JSON 3D array shape (nx)");
                }
                for (std::size_t x = 0; x < shape.nx; ++x) {
                    std::size_t idx = y * shape.nz * shape.nx + z * shape.nx + x;
                    data[idx] = row[x].get<float>();
                }
            }
        }
    } else {
        // 2D
        if (j.size() != shape.nz) {
            throw std::runtime_error("Invalid JSON 2D array shape (nz)");
        }
        for (std::size_t z = 0; z < shape.nz; ++z) {
            const auto& row = j[z];
            if (!row.is_array() || row.size() != shape.nx) {
                throw std::runtime_error("Invalid JSON 2D array shape (nx)");
            }
            for (std::size_t x = 0; x < shape.nx; ++x) {
                data[z * shape.nx + x] = row[x].get<float>();
            }
        }
    }
}

} // namespace io_bench
