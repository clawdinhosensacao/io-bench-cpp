#include "io_bench/formats.hpp"
#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cmath>
#include <nlohmann/json.hpp>

namespace io_bench {

namespace fs = std::filesystem;
using json = nlohmann::json;

// Zarr format: chunked binary with JSON metadata
// Each chunk is stored as a separate file

bool ZarrFormat::is_available() const {
    return true;  // Native implementation using filesystem + JSON
}

void ZarrFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // Create zarr directory structure
    fs::path zarr_path(path);
    fs::create_directories(zarr_path);
    
    // Write .zarray metadata
    json zarray;
    zarray["zarr_format"] = 2;
    zarray["shape"] = shape.ny > 1 
        ? json::array({shape.nz, shape.ny, shape.nx})
        : json::array({shape.nz, shape.nx});
    zarray["chunks"] = shape.ny > 1 
        ? json::array({std::min(shape.nz, (std::size_t)64), 
                       std::min(shape.ny, (std::size_t)64), 
                       std::min(shape.nx, (std::size_t)64)})
        : json::array({std::min(shape.nz, (std::size_t)64), 
                       std::min(shape.nx, (std::size_t)64)});
    zarray["dtype"] = "<f4";  // Little-endian float32
    zarray["compressor"] = nullptr;  // No compression
    zarray["fill_value"] = 0.0f;
    zarray["order"] = "C";
    zarray["filters"] = nullptr;
    
    std::ofstream meta_out(zarr_path / ".zarray");
    meta_out << zarray.dump(2);
    meta_out.close();
    
    // Determine chunk sizes
    std::size_t chunk_nz = std::min(shape.nz, (std::size_t)64);
    std::size_t chunk_ny = shape.ny > 1 ? std::min(shape.ny, (std::size_t)64) : 1;
    std::size_t chunk_nx = std::min(shape.nx, (std::size_t)64);
    
    // Write chunks
    std::size_t n_chunks_z = (shape.nz + chunk_nz - 1) / chunk_nz;
    std::size_t n_chunks_y = (shape.ny + chunk_ny - 1) / chunk_ny;
    std::size_t n_chunks_x = (shape.nx + chunk_nx - 1) / chunk_nx;
    
    for (std::size_t cz = 0; cz < n_chunks_z; ++cz) {
        for (std::size_t cy = 0; cy < n_chunks_y; ++cy) {
            for (std::size_t cx = 0; cx < n_chunks_x; ++cx) {
                // Build chunk filename
                std::ostringstream chunk_name;
                if (shape.ny > 1) {
                    chunk_name << cz << "/" << cy << "/" << cx;
                } else {
                    chunk_name << cz << "/" << cx;
                }
                
                fs::path chunk_path = zarr_path / chunk_name.str();
                fs::create_directories(chunk_path.parent_path());
                
                // Calculate actual chunk dimensions
                std::size_t start_z = cz * chunk_nz;
                std::size_t start_y = cy * chunk_ny;
                std::size_t start_x = cx * chunk_nx;
                
                std::size_t actual_nz = std::min(chunk_nz, shape.nz - start_z);
                std::size_t actual_ny = std::min(chunk_ny, shape.ny - start_y);
                std::size_t actual_nx = std::min(chunk_nx, shape.nx - start_x);
                
                // Write chunk data
                std::ofstream chunk_out(chunk_path, std::ios::binary);
                for (std::size_t iz = 0; iz < actual_nz; ++iz) {
                    for (std::size_t iy = 0; iy < actual_ny; ++iy) {
                        for (std::size_t ix = 0; ix < actual_nx; ++ix) {
                            std::size_t global_z = start_z + iz;
                            std::size_t global_y = start_y + iy;
                            std::size_t global_x = start_x + ix;
                            
                            std::size_t idx;
                            if (shape.ny > 1) {
                                idx = global_z * shape.ny * shape.nx + global_y * shape.nx + global_x;
                            } else {
                                idx = global_z * shape.nx + global_x;
                            }
                            
                            chunk_out.write(reinterpret_cast<const char*>(&data[idx]), sizeof(float));
                        }
                    }
                }
                chunk_out.close();
            }
        }
    }
}

void ZarrFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    fs::path zarr_path(path);
    
    // Read .zarray metadata
    std::ifstream meta_in(zarr_path / ".zarray");
    if (!meta_in) {
        throw std::runtime_error("Zarr: Cannot open .zarray metadata: " + path);
    }
    
    json zarray;
    meta_in >> zarray;
    meta_in.close();
    
    // Get chunk sizes
    auto chunks = zarray["chunks"];
    std::size_t chunk_nz = chunks[0].get<std::size_t>();
    std::size_t chunk_ny = shape.ny > 1 ? chunks[1].get<std::size_t>() : 1;
    std::size_t chunk_nx = shape.ny > 1 ? chunks[2].get<std::size_t>() : chunks[1].get<std::size_t>();
    
    // Read chunks
    std::size_t n_chunks_z = (shape.nz + chunk_nz - 1) / chunk_nz;
    std::size_t n_chunks_y = (shape.ny + chunk_ny - 1) / chunk_ny;
    std::size_t n_chunks_x = (shape.nx + chunk_nx - 1) / chunk_nx;
    
    for (std::size_t cz = 0; cz < n_chunks_z; ++cz) {
        for (std::size_t cy = 0; cy < n_chunks_y; ++cy) {
            for (std::size_t cx = 0; cx < n_chunks_x; ++cx) {
                // Build chunk filename
                std::ostringstream chunk_name;
                if (shape.ny > 1) {
                    chunk_name << cz << "/" << cy << "/" << cx;
                } else {
                    chunk_name << cz << "/" << cx;
                }
                
                fs::path chunk_path = zarr_path / chunk_name.str();
                
                if (!fs::exists(chunk_path)) {
                    // Chunk doesn't exist, fill with zeros (fill_value)
                    continue;
                }
                
                // Calculate actual chunk dimensions
                std::size_t start_z = cz * chunk_nz;
                std::size_t start_y = cy * chunk_ny;
                std::size_t start_x = cx * chunk_nx;
                
                std::size_t actual_nz = std::min(chunk_nz, shape.nz - start_z);
                std::size_t actual_ny = std::min(chunk_ny, shape.ny - start_y);
                std::size_t actual_nx = std::min(chunk_nx, shape.nx - start_x);
                
                // Read chunk data
                std::ifstream chunk_in(chunk_path, std::ios::binary);
                for (std::size_t iz = 0; iz < actual_nz; ++iz) {
                    for (std::size_t iy = 0; iy < actual_ny; ++iy) {
                        for (std::size_t ix = 0; ix < actual_nx; ++ix) {
                            std::size_t global_z = start_z + iz;
                            std::size_t global_y = start_y + iy;
                            std::size_t global_x = start_x + ix;
                            
                            std::size_t idx;
                            if (shape.ny > 1) {
                                idx = global_z * shape.ny * shape.nx + global_y * shape.nx + global_x;
                            } else {
                                idx = global_z * shape.nx + global_x;
                            }
                            
                            chunk_in.read(reinterpret_cast<char*>(&data[idx]), sizeof(float));
                        }
                    }
                }
                chunk_in.close();
            }
        }
    }
}

} // namespace io_bench
