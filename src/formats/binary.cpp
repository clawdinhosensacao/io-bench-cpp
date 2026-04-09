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

void BinaryFormat::read_slice(const std::string& path, float* slice_buf,
                               const ArrayShape& shape, std::size_t iy) {
    // Binary layout: data[iy][iz][ix] — seek to iy * nx * nz * sizeof(float)
    const std::size_t slice_elements = shape.nx * shape.nz;
    const std::size_t offset = iy * slice_elements * sizeof(float);
    
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for slice read: " + path);
    }
    file.seekg(static_cast<std::streamoff>(offset));
    file.read(reinterpret_cast<char*>(slice_buf), slice_elements * sizeof(float));
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

void MmapFormat::read_slice(const std::string& path, float* slice_buf,
                             const ArrayShape& shape, std::size_t iy) {
#ifdef __unix__
    const std::size_t slice_elements = shape.nx * shape.nz;
    const std::size_t slice_bytes = slice_elements * sizeof(float);
    const std::size_t offset = iy * slice_bytes;
    
    // Get page size for alignment
    const long page_size = sysconf(_SC_PAGESIZE);
    const std::size_t page_offset = offset % static_cast<std::size_t>(page_size);
    const std::size_t map_offset = offset - page_offset;
    const std::size_t map_size = slice_bytes + page_offset;
    
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("Cannot open file for mmap slice: " + path);
    }
    
    void* mapped = ::mmap(nullptr, map_size, PROT_READ, MAP_PRIVATE, fd, static_cast<off_t>(map_offset));
    if (mapped == MAP_FAILED) {
        ::close(fd);
        throw std::runtime_error("mmap slice failed for: " + path);
    }
    
    std::memcpy(slice_buf, static_cast<char*>(mapped) + page_offset, slice_bytes);
    
    ::munmap(mapped, map_size);
    ::close(fd);
#else
    // Fallback: seek and read
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for slice read: " + path);
    }
    const std::size_t slice_elements = shape.nx * shape.nz;
    const std::size_t offset = iy * slice_elements * sizeof(float);
    file.seekg(static_cast<std::streamoff>(offset));
    file.read(reinterpret_cast<char*>(slice_buf), slice_elements * sizeof(float));
#endif
}

} // namespace io_bench
