#include "io_bench/formats.hpp"
#include "io_bench/types.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace io_bench {

static std::string mseed_python() {
    return find_python_with_module("obspy");
}

static bool check_obspy_python() {
    return !mseed_python().empty();
}

static std::string write_temp_script(const std::string& content, const std::string& name) {
    std::string tmp_path = std::filesystem::temp_directory_path() / name;
    std::ofstream f(tmp_path);
    if (!f) { throw std::runtime_error("MiniSEED: cannot create temp script"); }
    f << content;
    f.close();
    return tmp_path;
}

bool MiniSeedFormat::is_available() const {
    return check_obspy_python();
}

void MiniSeedFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // Write raw binary first, then use Python (obspy) to write miniSEED
    const std::string tmp_bin = path + ".tmp_bin";

    {
        std::ofstream f(tmp_bin, std::ios::binary);
        if (!f) { throw std::runtime_error("MiniSEED: cannot write temp binary"); }
        f.write(reinterpret_cast<const char*>(data), to_ss(shape.bytes()));
    }

    // Python script: read binary → obspy Trace → write miniSEED
    std::ostringstream script;
    script << "import sys\n"
           << "import numpy as np\n"
           << "import obspy\n"
           << "\n"
           << "tmp_bin = sys.argv[1]\n"
           << "out_path = sys.argv[2]\n"
           << "ny, nz, nx = " << shape.ny << ", " << shape.nz << ", " << shape.nx << "\n"
           << "\n"
           << "data = np.fromfile(tmp_bin, dtype=np.float32)\n"
           << "tr = obspy.Trace(data=data)\n"
           << "tr.stats.network = 'XX'\n"
           << "tr.stats.station = 'BENCH'\n"
           << "tr.stats.channel = 'BHZ'\n"
           << "tr.stats.sampling_rate = 100.0\n"
           << "tr.write(out_path, format='MSEED')\n"
           << "print('OK')\n";

    std::string script_path = write_temp_script(script.str(), "mseed_bench_write.py");
    std::string cmd = mseed_python() + " " + script_path + " " + tmp_bin + " " + path + " 2>&1";
    int ret = std::system(cmd.c_str());  // NOLINT(bugprone-command-processor)

    std::remove(tmp_bin.c_str());
    std::remove(script_path.c_str());

    if (ret != 0) {
        throw std::runtime_error("MiniSEED write failed (Python subprocess)");
    }
}

void MiniSeedFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    // Use Python (obspy) to read miniSEED and dump to binary
    const std::string tmp_bin = path + ".tmp_read_bin";

    std::ostringstream script;
    script << "import sys\n"
           << "import numpy as np\n"
           << "import obspy\n"
           << "\n"
           << "mseed_path = sys.argv[1]\n"
           << "out_bin = sys.argv[2]\n"
           << "\n"
           << "st = obspy.read(mseed_path)\n"
           << "# Concatenate all traces (single trace expected for benchmark)\n"
           << "data = np.concatenate([tr.data for tr in st]).astype(np.float32)\n"
           << "data.tofile(out_bin)\n";

    std::string script_path = write_temp_script(script.str(), "mseed_bench_read.py");
    std::string cmd = mseed_python() + " " + script_path + " " + path + " " + tmp_bin + " 2>&1";
    int ret = std::system(cmd.c_str());  // NOLINT(bugprone-command-processor)

    if (ret != 0) {
        std::remove(tmp_bin.c_str());
        std::remove(script_path.c_str());
        throw std::runtime_error("MiniSEED read failed (Python subprocess)");
    }

    // Load the binary output
    {
        std::ifstream f(tmp_bin, std::ios::binary | std::ios::ate);
        if (!f) {
            std::remove(tmp_bin.c_str());
            std::remove(script_path.c_str());
            throw std::runtime_error("MiniSEED: cannot read temp binary output");
        }
        auto size = f.tellg();
        if (static_cast<std::size_t>(size) != shape.bytes()) {
            std::remove(tmp_bin.c_str());
            std::remove(script_path.c_str());
            throw std::runtime_error("MiniSEED: temp binary size mismatch (expected " +
                                     std::to_string(shape.bytes()) + ", got " +
                                     std::to_string(size) + ")");
        }
        f.seekg(0);
        f.read(reinterpret_cast<char*>(data), to_ss(shape.bytes()));
    }

    std::remove(tmp_bin.c_str());
    std::remove(script_path.c_str());
}

} // namespace io_bench
