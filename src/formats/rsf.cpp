#include "io_bench/formats.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace io_bench {

/// RSF (Regularly Sampled Format) from the Madagascar project.
///
/// The RSF format consists of a text header file and a binary data file.
/// The header contains key=value pairs describing the data dimensions and type.
/// The binary data is raw float32 in row-major order.
///
/// For the benchmark, we use a directory-based layout:
///   path/          (directory)
///     header.rsf   (text header with key=value pairs)
///     data.bin     (raw float32 binary data)
///
/// This ensures file size measurement works correctly with the benchmark runner.

void RsfFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // Create directory
    std::filesystem::create_directories(path);

    const std::string header_path = path + "/header.rsf";
    const std::string data_path = path + "/data.bin";

    // Write binary data
    {
        std::ofstream f(data_path, std::ios::binary);
        if (!f) throw std::runtime_error("RSF: cannot write data file: " + data_path);
        f.write(reinterpret_cast<const char*>(data), shape.bytes());
    }

    // Write text header
    std::ofstream hf(header_path);
    if (!hf) throw std::runtime_error("RSF: cannot write header file: " + header_path);

    // RSF convention: n1 is the fastest axis (x), n2 is next (z), n3 is slowest (y)
    hf << "n1=" << shape.nx << "\n";
    hf << "o1=0\n";
    hf << "d1=1\n";
    hf << "n2=" << shape.nz << "\n";
    hf << "o2=0\n";
    hf << "d2=1\n";
    if (shape.ny > 1) {
        hf << "n3=" << shape.ny << "\n";
        hf << "o3=0\n";
        hf << "d3=1\n";
    }
    hf << "data_format=native_float\n";
    hf << "esize=4\n";
    hf << "in=\"" << data_path << "\"\n";
}

void RsfFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    const std::string header_path = path + "/header.rsf";
    const std::string data_path = path + "/data.bin";

    // Verify header exists
    std::ifstream hf(header_path);
    if (!hf) throw std::runtime_error("RSF: cannot read header file: " + header_path);

    // Read binary data
    std::ifstream df(data_path, std::ios::binary | std::ios::ate);
    if (!df) throw std::runtime_error("RSF: cannot read data file: " + data_path);

    auto size = df.tellg();
    if (static_cast<std::size_t>(size) != shape.bytes()) {
        throw std::runtime_error("RSF: data file size mismatch (expected " +
                                 std::to_string(shape.bytes()) + ", got " +
                                 std::to_string(size) + ")");
    }

    df.seekg(0);
    df.read(reinterpret_cast<char*>(data), shape.bytes());
}

} // namespace io_bench
