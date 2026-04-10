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

static uint16_t read_be_u16(const uint8_t* buf) {
    return static_cast<uint16_t>((static_cast<uint16_t>(buf[0]) << 8) | buf[1]);
}

void SegDFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // Map the 2D/3D array as traces: each row (z-slice or yz-slice) is a trace
    const auto num_traces = static_cast<uint16_t>(shape.nz * shape.ny);  // NOLINT(modernize-use-auto)
    const auto num_samples = static_cast<uint16_t>(shape.nx);            // NOLINT(modernize-use-auto)
    const uint16_t sample_interval = 1000;  // 1ms = 1000 microseconds

    std::ofstream f(path, std::ios::binary);
    if (!f) { throw std::runtime_error("SEGD: cannot open output file: " + path); }

    // Write general header
    SegDGeneralHeader gh;
    std::memset(&gh, 0, sizeof(gh));
    gh.file_id[0] = 'S'; gh.file_id[1] = 'E'; gh.file_id[2] = 'G';
    gh.revision = 3;
    gh.format_code = 4;  // IEEE float32
    write_be_u16(reinterpret_cast<uint8_t*>(&gh.num_traces), num_traces);
    write_be_u16(reinterpret_cast<uint8_t*>(&gh.num_samples), num_samples);
    write_be_u16(reinterpret_cast<uint8_t*>(&gh.sample_interval), sample_interval);
    f.write(reinterpret_cast<const char*>(&gh), sizeof(gh));

    // Write traces
    for (uint16_t it = 0; it < num_traces; ++it) {
        SegDTraceHeader th;
        std::memset(&th, 0, sizeof(th));
        write_be_u32(th.trace_num, it + 1);
        write_be_u16(th.receiver_line, static_cast<uint16_t>((it / shape.nz) + 1));
        write_be_u16(th.receiver_num, static_cast<uint16_t>((it % shape.nz) + 1));
        f.write(reinterpret_cast<const char*>(&th), sizeof(th));

        // Write trace samples (big-endian float32)
        const float* trace_data = data + (static_cast<std::size_t>(it) * shape.nx);
        for (uint16_t isamp = 0; isamp < num_samples; ++isamp) {
            // Convert to big-endian float32
            uint32_t be_val;
            std::memcpy(&be_val, &trace_data[isamp], sizeof(float));
            be_val = __builtin_bswap32(be_val);
            f.write(reinterpret_cast<const char*>(&be_val), sizeof(be_val));
        }
    }
}

void SegDFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { throw std::runtime_error("SEGD: cannot open input file: " + path); }

    // Read general header
    SegDGeneralHeader gh;
    f.read(reinterpret_cast<char*>(&gh), sizeof(gh));
    if (!f) { throw std::runtime_error("SEGD: failed to read general header"); }

    const uint16_t num_traces = read_be_u16(reinterpret_cast<const uint8_t*>(&gh.num_traces));
    const uint16_t num_samples = read_be_u16(reinterpret_cast<const uint8_t*>(&gh.num_samples));

    // Read traces
    for (uint16_t it = 0; it < num_traces && it < (shape.nz * shape.ny); ++it) {
        SegDTraceHeader th;
        f.read(reinterpret_cast<char*>(&th), sizeof(th));
        if (!f) { throw std::runtime_error("SEGD: failed to read trace header"); }

        float* trace_data = data + (static_cast<std::size_t>(it) * shape.nx);
        for (uint16_t isamp = 0; isamp < num_samples && isamp < shape.nx; ++isamp) {
            uint32_t be_val;
            f.read(reinterpret_cast<char*>(&be_val), sizeof(be_val));
            if (!f) { throw std::runtime_error("SEGD: failed to read trace sample"); }
            be_val = __builtin_bswap32(be_val);
            std::memcpy(&trace_data[isamp], &be_val, sizeof(float));
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

    // Read general header to get num_samples
    SegDGeneralHeader gh;
    f.read(reinterpret_cast<char*>(&gh), sizeof(gh));
    const uint16_t num_samples = read_be_u16(reinterpret_cast<const uint8_t*>(&gh.num_samples));

    // Seek directly to the requested trace
    const std::size_t trace_offset = sizeof(gh) + (trace_idx * (sizeof(SegDTraceHeader) + static_cast<std::size_t>(num_samples) * sizeof(float)));
    f.seekg(static_cast<std::streamsize>(trace_offset), std::ios::beg);

    // Skip trace header
    SegDTraceHeader th;
    f.read(reinterpret_cast<char*>(&th), sizeof(th));

    // Read trace samples (big-endian float32)
    for (uint16_t isamp = 0; isamp < num_samples && isamp < shape.nx; ++isamp) {
        uint32_t be_val;
        f.read(reinterpret_cast<char*>(&be_val), sizeof(be_val));
        if (!f) { throw std::runtime_error("SEGD: failed to read trace sample"); }
        be_val = __builtin_bswap32(be_val);
        std::memcpy(&trace_buf[isamp], &be_val, sizeof(float));
    }
}

void SegDFormat::write_trace(const std::string& path, const float* trace_data,
                             const ArrayShape& shape, std::size_t trace_idx) {
    const auto num_samples = static_cast<uint16_t>(shape.nx);

    if (trace_idx == 0) {
        // First trace: create file with general header
        std::ofstream f(path, std::ios::binary);
        if (!f) { throw std::runtime_error("SEGD: cannot open file for streaming write: " + path); }

        SegDGeneralHeader gh;
        std::memset(&gh, 0, sizeof(gh));
        gh.file_id[0] = 'S'; gh.file_id[1] = 'E'; gh.file_id[2] = 'G';
        gh.revision = 3;
        gh.format_code = 4;  // IEEE float32
        const auto total_traces = static_cast<uint16_t>(shape.nz * shape.ny);
        write_be_u16(reinterpret_cast<uint8_t*>(&gh.num_traces), total_traces);
        write_be_u16(reinterpret_cast<uint8_t*>(&gh.num_samples), num_samples);
        write_be_u16(reinterpret_cast<uint8_t*>(&gh.sample_interval), 1000);
        f.write(reinterpret_cast<const char*>(&gh), sizeof(gh));

        // Write first trace header + data
        SegDTraceHeader th;
        std::memset(&th, 0, sizeof(th));
        write_be_u32(th.trace_num, 1);
        write_be_u16(th.receiver_line, 1);
        write_be_u16(th.receiver_num, 1);
        f.write(reinterpret_cast<const char*>(&th), sizeof(th));

        for (uint16_t isamp = 0; isamp < num_samples; ++isamp) {
            uint32_t be_val;
            std::memcpy(&be_val, &trace_data[isamp], sizeof(float));
            be_val = __builtin_bswap32(be_val);
            f.write(reinterpret_cast<const char*>(&be_val), sizeof(be_val));
        }
    } else {
        // Subsequent traces: append
        std::ofstream f(path, std::ios::binary | std::ios::app);
        if (!f) { throw std::runtime_error("SEGD: cannot open file for streaming append: " + path); }

        SegDTraceHeader th;
        std::memset(&th, 0, sizeof(th));
        write_be_u32(th.trace_num, static_cast<uint32_t>(trace_idx + 1));
        write_be_u16(th.receiver_line, static_cast<uint16_t>((trace_idx / shape.nz) + 1));
        write_be_u16(th.receiver_num, static_cast<uint16_t>((trace_idx % shape.nz) + 1));
        f.write(reinterpret_cast<const char*>(&th), sizeof(th));

        for (uint16_t isamp = 0; isamp < num_samples; ++isamp) {
            uint32_t be_val;
            std::memcpy(&be_val, &trace_data[isamp], sizeof(float));
            be_val = __builtin_bswap32(be_val);
            f.write(reinterpret_cast<const char*>(&be_val), sizeof(be_val));
        }
    }
}

} // namespace io_bench
