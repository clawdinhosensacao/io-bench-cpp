#include "io_bench/formats.hpp"
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <cstdint>

namespace io_bench {

// SEG-Y format constants
// Standard SEG-Y uses 3200-byte text header + 400-byte binary header
// Each trace has 240-byte trace header + samples

constexpr std::size_t TEXT_HEADER_SIZE = 3200;
constexpr std::size_t BINARY_HEADER_SIZE = 400;
constexpr std::size_t TRACE_HEADER_SIZE = 240;

bool SegyFormat::is_available() const {
    return true;  // Native implementation, no external deps
}

void SegyFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("SEG-Y: Cannot open file for writing: " + path);
    }
    
    // Write text header (3200 bytes of spaces)
    std::vector<char> text_header(TEXT_HEADER_SIZE, ' ');
    out.write(text_header.data(), TEXT_HEADER_SIZE);
    
    // Write binary header (400 bytes)
    std::vector<char> binary_header(BINARY_HEADER_SIZE, 0);
    
    // Number of samples per trace (bytes 3224-3225, big-endian)
    auto nsamples = static_cast<uint16_t>(shape.nz);
    binary_header[22] = static_cast<char>((nsamples >> 8) & 0xFF);
    binary_header[23] = static_cast<char>(nsamples & 0xFF);
    
    // Sample interval in microseconds (bytes 3216-3217)
    uint16_t dt = 1000;  // 1ms default
    binary_header[16] = static_cast<char>((dt >> 8) & 0xFF);
    binary_header[17] = static_cast<char>(dt & 0xFF);
    
    // Data format: IEEE float (bytes 3225-3226)
    // Format code 5 = IEEE float
    binary_header[24] = 0;
    binary_header[25] = 5;
    
    // Dimensionality indicator (bytes 3505-3506): 1 = 3D poststack
    // For 2D: number of traces in line
    
    out.write(binary_header.data(), BINARY_HEADER_SIZE);
    
    // Write traces
    // For 2D data (ny=1): each x position is a trace
    // For 3D data: each (x, y) position is a trace
    
    std::size_t num_traces = shape.nx * shape.ny;
    std::size_t samples_per_trace = shape.nz;
    
    for (std::size_t trace_idx = 0; trace_idx < num_traces; ++trace_idx) {
        // Write trace header (240 bytes)
        std::vector<char> trace_header(TRACE_HEADER_SIZE, 0);
        
        // Trace sequence number (bytes 1-4)
        auto seq = static_cast<uint32_t>(trace_idx + 1);
        trace_header[0] = static_cast<char>((seq >> 24) & 0xFF);
        trace_header[1] = static_cast<char>((seq >> 16) & 0xFF);
        trace_header[2] = static_cast<char>((seq >> 8) & 0xFF);
        trace_header[3] = static_cast<char>(seq & 0xFF);
        
        // CDP X coordinate (bytes 81-84)
        // For simplicity, use trace index * 100
        auto cdp_x = static_cast<int32_t>((trace_idx % shape.nx) * 100);
        trace_header[80] = static_cast<char>((cdp_x >> 24) & 0xFF);
        trace_header[81] = static_cast<char>((cdp_x >> 16) & 0xFF);
        trace_header[82] = static_cast<char>((cdp_x >> 8) & 0xFF);
        trace_header[83] = static_cast<char>(cdp_x & 0xFF);
        
        // CDP Y coordinate (bytes 85-88)
        int32_t cdp_y = static_cast<int32_t>((trace_idx / shape.nx) * 100);
        trace_header[84] = static_cast<char>((cdp_y >> 24) & 0xFF);
        trace_header[85] = static_cast<char>((cdp_y >> 16) & 0xFF);
        trace_header[86] = static_cast<char>((cdp_y >> 8) & 0xFF);
        trace_header[87] = static_cast<char>(cdp_y & 0xFF);
        
        // Number of samples (bytes 115-116)
        uint16_t ns = static_cast<uint16_t>(samples_per_trace);
        trace_header[114] = static_cast<char>((ns >> 8) & 0xFF);
        trace_header[115] = static_cast<char>(ns & 0xFF);
        
        out.write(trace_header.data(), TRACE_HEADER_SIZE);
        
        // Write trace samples (IEEE float, big-endian)
        for (std::size_t iz = 0; iz < samples_per_trace; ++iz) {
            std::size_t idx;
            if (shape.ny > 1) {
                std::size_t ix = trace_idx % shape.nx;
                std::size_t iy = trace_idx / shape.nx;
                idx = iz * shape.ny * shape.nx + iy * shape.nx + ix;
            } else {
                std::size_t ix = trace_idx;
                idx = iz * shape.nx + ix;
            }
            
            float val = data[idx];
            // Convert to big-endian
            uint32_t bits;
            std::memcpy(&bits, &val, sizeof(float));
            char bytes[4];
            bytes[0] = static_cast<char>((bits >> 24) & 0xFF);
            bytes[1] = static_cast<char>((bits >> 16) & 0xFF);
            bytes[2] = static_cast<char>((bits >> 8) & 0xFF);
            bytes[3] = static_cast<char>(bits & 0xFF);
            out.write(bytes, 4);
        }
    }
}

void SegyFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("SEG-Y: Cannot open file for reading: " + path);
    }
    
    // Skip text header
    in.seekg(TEXT_HEADER_SIZE, std::ios::beg);
    
    // Read binary header
    std::vector<char> binary_header(BINARY_HEADER_SIZE);
    in.read(binary_header.data(), BINARY_HEADER_SIZE);
    
    // Get number of samples per trace
    uint16_t nsamples = (static_cast<uint16_t>(static_cast<unsigned char>(binary_header[22])) << 8) |
                        static_cast<uint16_t>(static_cast<unsigned char>(binary_header[23]));
    
    if (nsamples != shape.nz) {
        throw std::runtime_error("SEG-Y: Sample count mismatch");
    }
    
    // Get data format
    uint16_t format = (static_cast<uint16_t>(static_cast<unsigned char>(binary_header[24])) << 8) |
                      static_cast<uint16_t>(static_cast<unsigned char>(binary_header[25]));
    
    if (format != 5) {
        throw std::runtime_error("SEG-Y: Only IEEE float format (5) is supported");
    }
    
    // Read traces
    std::size_t num_traces = shape.nx * shape.ny;
    
    for (std::size_t trace_idx = 0; trace_idx < num_traces; ++trace_idx) {
        // Skip trace header
        in.seekg(TRACE_HEADER_SIZE, std::ios::cur);
        
        // Read trace samples
        for (std::size_t iz = 0; iz < shape.nz; ++iz) {
            char bytes[4];
            in.read(bytes, 4);
            
            // Convert from big-endian
            uint32_t bits = (static_cast<uint32_t>(static_cast<unsigned char>(bytes[0])) << 24) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(bytes[1])) << 16) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(bytes[2])) << 8) |
                           static_cast<uint32_t>(static_cast<unsigned char>(bytes[3]));
            
            float val;
            std::memcpy(&val, &bits, sizeof(float));
            
            std::size_t idx;
            if (shape.ny > 1) {
                std::size_t ix = trace_idx % shape.nx;
                std::size_t iy = trace_idx / shape.nx;
                idx = iz * shape.ny * shape.nx + iy * shape.nx + ix;
            } else {
                std::size_t ix = trace_idx;
                idx = iz * shape.nx + ix;
            }
            data[idx] = val;
        }
    }
}

} // namespace io_bench
