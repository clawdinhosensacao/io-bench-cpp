#include "io_bench/formats.hpp"
#include "io_bench/types.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace io_bench {

static std::string mdio_python() {
    return find_python_with_module("mdio");
}

static bool check_mdio_python() {
    return !mdio_python().empty();
}

static std::string write_temp_script(const std::string& content, const std::string& name) {
    std::string tmp_path = std::filesystem::temp_directory_path() / name;
    std::ofstream f(tmp_path);
    if (!f) { throw std::runtime_error("MDIO: cannot create temp script"); }
    f << content;
    f.close();
    return tmp_path;
}

bool MdioFormat::is_available() const {
    return check_mdio_python();
}

void MdioFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // Write raw binary first, then use Python to ingest via MDIO
    const std::string tmp_bin = path + ".tmp_bin";

    // Write raw binary
    {
        std::ofstream f(tmp_bin, std::ios::binary);
        if (!f) { throw std::runtime_error("MDIO: cannot write temp binary"); }
        f.write(reinterpret_cast<const char*>(data), to_ss(shape.bytes()));
    }

    // Python script to read binary and write via MDIO
    std::ostringstream script;
    script << "import sys\n"
           << "import numpy as np\n"
           << "import xarray as xr\n"
           << "from mdio import to_mdio\n"
           << "\n"
           << "tmp_bin = sys.argv[1]\n"
           << "out_path = sys.argv[2]\n"
           << "ny, nz, nx = " << shape.ny << ", " << shape.nz << ", " << shape.nx << "\n"
           << "\n"
           << "data = np.fromfile(tmp_bin, dtype=np.float32)\n";
    if (shape.is_3d()) {
        script << "data = data.reshape(ny, nz, nx)\n"
               << "ds = xr.Dataset({'velocity': xr.DataArray(data, dims=['y', 'z', 'x'])})\n";
    } else {
        script << "data = data.reshape(nz, nx)\n"
               << "ds = xr.Dataset({'velocity': xr.DataArray(data, dims=['z', 'x'])})\n";
    }
    script << "\n"
           << "import shutil\n"
           << "if __import__('os').path.exists(out_path):\n"
           << "    shutil.rmtree(out_path)\n"
           << "to_mdio(ds, out_path)\n"
           << "print('OK')\n";

    std::string script_path = write_temp_script(script.str(), "mdio_bench_write.py");
    std::string cmd = mdio_python() + " " + script_path + " " + tmp_bin + " " + path + " 2>&1";
    int ret = std::system(cmd.c_str());  // NOLINT(bugprone-command-processor)

    // Clean up
    std::remove(tmp_bin.c_str());
    std::remove(script_path.c_str());

    if (ret != 0) {
        throw std::runtime_error("MDIO write failed (Python subprocess)");
    }
}

void MdioFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    // Use Python to read via MDIO and dump to binary, then load
    const std::string tmp_bin = path + ".tmp_read_bin";

    std::ostringstream script;
    script << "import sys\n"
           << "import numpy as np\n"
           << "from mdio import open_mdio\n"
           << "\n"
           << "mdio_path = sys.argv[1]\n"
           << "out_bin = sys.argv[2]\n"
           << "\n"
           << "ds = open_mdio(mdio_path)\n"
           << "# Get the first data variable\n"
           << "var_name = list(ds.data_vars)[0]\n"
           << "data = ds[var_name].values.astype(np.float32)\n"
           << "data.tofile(out_bin)\n";

    std::string script_path = write_temp_script(script.str(), "mdio_bench_read.py");
    std::string cmd = mdio_python() + " " + script_path + " " + path + " " + tmp_bin + " 2>&1";
    int ret = std::system(cmd.c_str());  // NOLINT(bugprone-command-processor)

    if (ret != 0) {
        std::remove(tmp_bin.c_str());
        std::remove(script_path.c_str());
        throw std::runtime_error("MDIO read failed (Python subprocess)");
    }

    // Load the binary output
    {
        std::ifstream f(tmp_bin, std::ios::binary | std::ios::ate);
        if (!f) {
            std::remove(tmp_bin.c_str());
            std::remove(script_path.c_str());
            throw std::runtime_error("MDIO: cannot read temp binary output");
        }
        auto size = f.tellg();
        if (static_cast<std::size_t>(size) != shape.bytes()) {
            std::remove(tmp_bin.c_str());
            std::remove(script_path.c_str());
            throw std::runtime_error("MDIO: temp binary size mismatch (expected " +
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
