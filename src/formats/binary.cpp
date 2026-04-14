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
    file.write(reinterpret_cast<const char*>(data), to_ss(shape.bytes()));
}

void BinaryFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for reading: " + path);
    }
    file.read(reinterpret_cast<char*>(data), to_ss(shape.bytes()));
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
    file.read(reinterpret_cast<char*>(slice_buf), to_ss(slice_elements * sizeof(float)));
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
    file.write(reinterpret_cast<const char*>(data), to_ss(shape.bytes()));
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
    file.read(reinterpret_cast<char*>(data), to_ss(shape.bytes()));
}

void MmapFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // For mmap, write is the same as binary
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    file.write(reinterpret_cast<const char*>(data), to_ss(shape.bytes()));
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
    file.read(reinterpret_cast<char*>(data), to_ss(shape.bytes()));
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
    file.read(reinterpret_cast<char*>(slice_buf), to_ss(slice_elements * sizeof(float)));
#endif
}

// ============================================================================
// Direct I/O Format (O_DIRECT — bypass page cache)
// ============================================================================

/// Align buffer to the system's logical sector size for O_DIRECT
static std::size_t get_direct_alignment() {
    return 4096;  // Common alignment for O_DIRECT on most Linux filesystems
}

/// Allocate aligned memory for O_DIRECT operations
static void* alloc_aligned(std::size_t size) {
    void* ptr = nullptr;
    if (posix_memalign(&ptr, get_direct_alignment(), size) != 0) {
        throw std::runtime_error("DirectIO: failed to allocate aligned memory");
    }
    return ptr;
}

bool DirectIOFormat::is_available() const {
#ifdef __linux__
    return true;
#else
    return false;
#endif
}

void DirectIOFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
#ifdef __linux__
    const std::size_t alignment = get_direct_alignment();
    const std::size_t raw_bytes = shape.bytes();
    const std::size_t aligned_bytes = ((raw_bytes + alignment - 1) / alignment) * alignment;

    void* buf = alloc_aligned(aligned_bytes);
    std::memset(buf, 0, aligned_bytes);
    std::memcpy(buf, data, raw_bytes);

    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_DIRECT | O_TRUNC, 0644);
    if (fd < 0) {
        free(buf);
        throw std::runtime_error("DirectIO: cannot open file for writing: " + path);
    }

    ssize_t written = ::write(fd, buf, aligned_bytes);
    ::close(fd);
    free(buf);

    if (written < 0 || static_cast<std::size_t>(written) < raw_bytes) {  // NOLINT(modernize-use-integer-sign-comparison)
        throw std::runtime_error("DirectIO: write failed for: " + path);
    }

    // Truncate to actual size (remove alignment padding)
    ::truncate(path.c_str(), static_cast<off_t>(raw_bytes));
#else
    (void)path; (void)data; (void)shape;
    throw std::runtime_error("DirectIO: not supported on this platform");
#endif
}

void DirectIOFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef __linux__
    const std::size_t alignment = get_direct_alignment();
    const std::size_t raw_bytes = shape.bytes();
    const std::size_t aligned_bytes = ((raw_bytes + alignment - 1) / alignment) * alignment;

    void* buf = alloc_aligned(aligned_bytes);

    int fd = ::open(path.c_str(), O_RDONLY | O_DIRECT);
    if (fd < 0) {
        free(buf);
        throw std::runtime_error("DirectIO: cannot open file for reading: " + path);
    }

    ssize_t nread = ::read(fd, buf, aligned_bytes);
    ::close(fd);

    if (nread < 0) {
        free(buf);
        throw std::runtime_error("DirectIO: read failed for: " + path);
    }

    std::memcpy(data, buf, raw_bytes);
    free(buf);
#else
    (void)path; (void)data; (void)shape;
    throw std::runtime_error("DirectIO: not supported on this platform");
#endif
}

void DirectIOFormat::read_slice(const std::string& path, float* slice_buf,
                                 const ArrayShape& shape, std::size_t iy) {
#ifdef __linux__
    const std::size_t alignment = get_direct_alignment();
    const std::size_t slice_elements = shape.nx * shape.nz;
    const std::size_t raw_bytes = slice_elements * sizeof(float);
    const std::size_t aligned_bytes = ((raw_bytes + alignment - 1) / alignment) * alignment;
    const std::size_t offset = iy * raw_bytes;

    // Offset must be aligned for O_DIRECT
    const std::size_t aligned_offset = (offset / alignment) * alignment;
    const std::size_t offset_diff = offset - aligned_offset;

    void* buf = alloc_aligned(aligned_bytes + alignment);

    int fd = ::open(path.c_str(), O_RDONLY | O_DIRECT);
    if (fd < 0) {
        free(buf);
        throw std::runtime_error("DirectIO: cannot open file for slice read: " + path);
    }

    ssize_t nread = ::pread(fd, buf, aligned_bytes + alignment, static_cast<off_t>(aligned_offset));
    ::close(fd);

    if (nread < 0) {
        free(buf);
        throw std::runtime_error("DirectIO: slice read failed for: " + path);
    }

    std::memcpy(slice_buf, static_cast<char*>(buf) + offset_diff, raw_bytes);
    free(buf);
#else
    (void)path; (void)slice_buf; (void)shape; (void)iy;
    throw std::runtime_error("DirectIO: not supported on this platform");
#endif
}

} // namespace io_bench
