#include "io_bench/formats.hpp"

#include <fstream>
#include <stdexcept>
#include <cstring>

// POSIX headers for mmap
#ifdef __unix__
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

namespace io_bench {

void BinaryFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    file.write(reinterpret_cast<const char*>(data), shape.bytes());
}

void BinaryFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for reading: " + path);
    }
    file.read(reinterpret_cast<char*>(data), shape.bytes());
}

// Binary with header: [magic][nx][nz][data]
// Header: 4 bytes magic "BINX" + 8 bytes nx + 8 bytes nz = 20 bytes
static constexpr const char* BIN_MAGIC = "BINX";

void BinaryHeaderFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    // Write header: magic + nx + nz + ny (for 3D)
    file.write(BIN_MAGIC, 4);
    auto nx = static_cast<uint64_t>(shape.nx);
    auto nz = static_cast<uint64_t>(shape.nz);
    auto ny = static_cast<uint64_t>(shape.ny);
    file.write(reinterpret_cast<const char*>(&nx), sizeof(nx));
    file.write(reinterpret_cast<const char*>(&nz), sizeof(nz));
    file.write(reinterpret_cast<const char*>(&ny), sizeof(ny));
    // Write data
    file.write(reinterpret_cast<const char*>(data), shape.bytes());
}

void BinaryHeaderFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for reading: " + path);
    }
    // Read and verify header
    char magic[4];
    file.read(magic, 4);
    if (std::strncmp(magic, BIN_MAGIC, 4) != 0) {
        throw std::runtime_error("Invalid binary header magic in: " + path);
    }
    uint64_t nx;
    uint64_t nz;
    uint64_t ny;
    file.read(reinterpret_cast<char*>(&nx), sizeof(nx));
    file.read(reinterpret_cast<char*>(&nz), sizeof(nz));
    file.read(reinterpret_cast<char*>(&ny), sizeof(ny));
    if (nx != shape.nx || nz != shape.nz || ny != shape.ny) {
        throw std::runtime_error("Shape mismatch in binary header");
    }
    // Read data
    file.read(reinterpret_cast<char*>(data), shape.bytes());
}

void MmapFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // For mmap, write is the same as binary
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    file.write(reinterpret_cast<const char*>(data), shape.bytes());
}

void MmapFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef __unix__
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("Cannot open file for mmap: " + path);
    }
    
    void* mapped = ::mmap(nullptr, shape.bytes(), PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        ::close(fd);
        throw std::runtime_error("mmap failed for: " + path);
    }
    
    // Copy data (simulating typical use case)
    std::memcpy(data, mapped, shape.bytes());
    
    ::munmap(mapped, shape.bytes());
    ::close(fd);
#else
    // Fallback to regular read on non-Unix
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for reading: " + path);
    }
    file.read(reinterpret_cast<char*>(data), shape.bytes());
#endif
}

} // namespace io_bench
