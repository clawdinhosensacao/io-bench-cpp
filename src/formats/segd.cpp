#include "io_bench/formats.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace io_bench {

/// SEGD (SEG-D) format adapter — native C++ implementation.
///
/// SEGD is the field recording format used during seismic data acquisition.
/// It is structurally similar to SEG-Y (trace headers + trace data) but uses
/// different header layouts optimized for recording rather than processing.
///
/// This implementation writes a simplified SEGD Rev 3.0 compatible file:
/// - General header block (96 bytes): record ID, format code, sample interval, num samples
/// - Per-trace: 20-byte trace header + float32 samples
/// - Format code 4 = IEEE float32 (same as SEG-Y rev 1)

#pragma pack(push, 1)
struct SegDGeneralHeader {
    uint8_t  file_id[3];       // "SEG" identifier
    uint8_t  revision;         // Format revision (3 = Rev 3.0)
    uint8_t  format_code;      // 4 = IEEE float32
    uint16_t num_traces;       // Number of traces (big-endian)
    uint16_t num_samples;      // Samples per trace (big-endian)
    uint16_t sample_interval;  // Sample interval in microseconds (big-endian)
    uint8_t  reserved[84];     // Padding to 96 bytes total
};

struct SegDTraceHeader {
    uint8_t  trace_num[4];     // Trace sequence number (big-endian)
    uint8_t  receiver_line[2]; // Receiver line number (big-endian)
    uint8_t  receiver_num[2];  // Receiver number (big-endian)
    uint8_t  reserved[12];     // Padding to 20 bytes total
};
#pragma pack(pop)

// --- Portable big-endian I/O helpers ---

static void write_be_u16(uint8_t* buf, uint16_t val) {
    buf[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[1] = static_cast<uint8_t>(val & 0xFF);
}

static void write_be_u32(uint8_t* buf, uint32_t val) {
    buf[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(val & 0xFF);
}

static auto read_be_u16(const uint8_t* buf) -> uint16_t {
    return static_cast<uint16_t>((static_cast<uint16_t>(buf[0]) << 8) | buf[1]);
}

/// Swap byte order of a 32-bit value (portable, no compiler intrinsics)
static auto byte_swap_u32(uint32_t val) -> uint32_t {
    return ((val >> 24) & 0x000000FFU) |
           ((val >>  8) & 0x0000FF00U) |
           ((val <<  8) & 0x00FF0000U) |
           ((val << 24) & 0xFF000000U);
}

/// Write a float in big-endian byte order
static void write_be_float(std::ofstream& f, float val) {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(float));
    bits = byte_swap_u32(bits);
    f.write(reinterpret_cast<const char*>(&bits), sizeof(bits));
}

/// Read a float from big-endian byte order
static auto read_be_float(std::ifstream& f) -> float {
    uint32_t bits;
    f.read(reinterpret_cast<char*>(&bits), sizeof(bits));
    bits = byte_swap_u32(bits);
    float val;
    std::memcpy(&val, &bits, sizeof(float));
    return val;
}

// --- SEGD format implementation ---

void SegDFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    const auto num_traces = static_cast<uint16_t>(shape.nz * shape.ny);
    const auto num_samples = static_cast<uint16_t>(shape.nx);
    const uint16_t sample_interval = 1000;  // 1ms = 1000 microseconds

    std::ofstream f(path, std::ios::binary);
    if (!f) { throw std::runtime_error("SEGD: cannot open output file: " + path); }

    // Write general header
    SegDGeneralHeader gh{};
    gh.file_id[0] = 'S'; gh.file_id[1] = 'E'; gh.file_id[2] = 'G';
    gh.revision = 3;
    gh.format_code = 4;  // IEEE float32
    write_be_u16(reinterpret_cast<uint8_t*>(&gh.num_traces), num_traces);
    write_be_u16(reinterpret_cast<uint8_t*>(&gh.num_samples), num_samples);
    write_be_u16(reinterpret_cast<uint8_t*>(&gh.sample_interval), sample_interval);
    f.write(reinterpret_cast<const char*>(&gh), sizeof(gh));

    // Write traces
    for (uint16_t it = 0; it < num_traces; ++it) {
        SegDTraceHeader th{};
        write_be_u32(th.trace_num, it + 1);
        write_be_u16(th.receiver_line, static_cast<uint16_t>((it / shape.nz) + 1));
        write_be_u16(th.receiver_num, static_cast<uint16_t>((it % shape.nz) + 1));
        f.write(reinterpret_cast<const char*>(&th), sizeof(th));

        const float* trace_data = data + (static_cast<std::size_t>(it) * shape.nx);
        for (uint16_t isamp = 0; isamp < num_samples; ++isamp) {
            write_be_float(f, trace_data[isamp]);
        }
    }
}

void SegDFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { throw std::runtime_error("SEGD: cannot open input file: " + path); }

    SegDGeneralHeader gh;
    f.read(reinterpret_cast<char*>(&gh), sizeof(gh));
    if (!f) { throw std::runtime_error("SEGD: failed to read general header"); }

    const uint16_t num_traces = read_be_u16(reinterpret_cast<const uint8_t*>(&gh.num_traces));
    const uint16_t num_samples = read_be_u16(reinterpret_cast<const uint8_t*>(&gh.num_samples));

    for (uint16_t it = 0; it < num_traces && it < (shape.nz * shape.ny); ++it) {
        SegDTraceHeader th;
        f.read(reinterpret_cast<char*>(&th), sizeof(th));
        if (!f) { throw std::runtime_error("SEGD: failed to read trace header"); }

        float* trace_data = data + (static_cast<std::size_t>(it) * shape.nx);
        for (uint16_t isamp = 0; isamp < num_samples && isamp < shape.nx; ++isamp) {
            trace_data[isamp] = read_be_float(f);
            if (!f) { throw std::runtime_error("SEGD: failed to read trace sample"); }
        }
        // Skip remaining samples if num_samples > shape.nx
        if (num_samples > shape.nx) {
            f.seekg(static_cast<std::streamsize>((num_samples - shape.nx)) * sizeof(float), std::ios::cur);
        }
    }
}

void SegDFormat::read_trace(const std::string& path, float* trace_buf,
                            const ArrayShape& shape, std::size_t trace_idx) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { throw std::runtime_error("SEGD: cannot open input file: " + path); }

    SegDGeneralHeader gh;
    f.read(reinterpret_cast<char*>(&gh), sizeof(gh));
    const uint16_t num_samples = read_be_u16(reinterpret_cast<const uint8_t*>(&gh.num_samples));

    const auto trace_offset = static_cast<std::size_t>(
        sizeof(gh) + (trace_idx * (sizeof(SegDTraceHeader) + static_cast<std::size_t>(num_samples) * sizeof(float))));
    f.seekg(static_cast<std::streamsize>(trace_offset), std::ios::beg);

    SegDTraceHeader th;
    f.read(reinterpret_cast<char*>(&th), sizeof(th));

    for (uint16_t isamp = 0; isamp < num_samples && isamp < shape.nx; ++isamp) {
        trace_buf[isamp] = read_be_float(f);
        if (!f) { throw std::runtime_error("SEGD: failed to read trace sample"); }
    }
}

void SegDFormat::write_trace(const std::string& path, const float* trace_data,
                             const ArrayShape& shape, std::size_t trace_idx) {
    const auto num_samples = static_cast<uint16_t>(shape.nx);

    if (trace_idx == 0) {
        std::ofstream f(path, std::ios::binary);
        if (!f) { throw std::runtime_error("SEGD: cannot open file for streaming write: " + path); }

        SegDGeneralHeader gh{};
        gh.file_id[0] = 'S'; gh.file_id[1] = 'E'; gh.file_id[2] = 'G';
        gh.revision = 3;
        gh.format_code = 4;
        const auto total_traces = static_cast<uint16_t>(shape.nz * shape.ny);
        write_be_u16(reinterpret_cast<uint8_t*>(&gh.num_traces), total_traces);
        write_be_u16(reinterpret_cast<uint8_t*>(&gh.num_samples), num_samples);
        write_be_u16(reinterpret_cast<uint8_t*>(&gh.sample_interval), 1000);
        f.write(reinterpret_cast<const char*>(&gh), sizeof(gh));

        SegDTraceHeader th{};
        write_be_u32(th.trace_num, 1);
        write_be_u16(th.receiver_line, 1);
        write_be_u16(th.receiver_num, 1);
        f.write(reinterpret_cast<const char*>(&th), sizeof(th));

        for (uint16_t isamp = 0; isamp < num_samples; ++isamp) {
            write_be_float(f, trace_data[isamp]);
        }
    } else {
        std::ofstream f(path, std::ios::binary | std::ios::app);
        if (!f) { throw std::runtime_error("SEGD: cannot open file for streaming append: " + path); }

        SegDTraceHeader th{};
        write_be_u32(th.trace_num, static_cast<uint32_t>(trace_idx + 1));
        write_be_u16(th.receiver_line, static_cast<uint16_t>((trace_idx / shape.nz) + 1));
        write_be_u16(th.receiver_num, static_cast<uint16_t>((trace_idx % shape.nz) + 1));
        f.write(reinterpret_cast<const char*>(&th), sizeof(th));

        for (uint16_t isamp = 0; isamp < num_samples; ++isamp) {
            write_be_float(f, trace_data[isamp]);
        }
    }
}

} // namespace io_bench
