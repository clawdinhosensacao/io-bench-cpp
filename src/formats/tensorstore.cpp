#include "io_bench/formats.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace io_bench {

bool TensorStoreFormat::is_available() const {
    // TensorStore has no public C++ API; use Python bridge
    // Check if the Python tensorstore module is importable
    int ret = std::system("python3 -c 'import tensorstore' 2>/dev/null");  // NOLINT(bugprone-command-processor)
    return ret == 0;
}

static std::string write_temp_script(const std::string& content) {
    // Create a temp Python script file to avoid shell quoting issues
    std::string tmp_path = std::filesystem::temp_directory_path() / "ts_bench_write.py";
    std::ofstream f(tmp_path);
    if (!f) { throw std::runtime_error("TensorStore: cannot create temp script"); }
    f << content;
    f.close();
    return tmp_path;
}

void TensorStoreFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // Write data as raw binary first, then use Python to ingest via TensorStore
    const std::string tmp_bin = path + ".tmp_bin";

    // Write raw binary
    {
        std::ofstream f(tmp_bin, std::ios::binary);
        if (!f) { throw std::runtime_error("TensorStore: cannot write temp binary"); }
        f.write(reinterpret_cast<const char*>(data), shape.bytes());
    }

    // Python script to read binary and write via TensorStore (zarr driver)
    std::ostringstream script;
    script << "import tensorstore as ts\n"
           << "import numpy as np\n"
           << "import sys\n"
           << "tmp_bin = sys.argv[1]\n"
           << "out_path = sys.argv[2]\n"
           << "ny, nz, nx = " << shape.ny << ", " << shape.nz << ", " << shape.nx << "\n"
           << "data = np.fromfile(tmp_bin, dtype=np.float32).reshape(ny, nz, nx)\n"
           << "store = ts.open({\n"
           << "    'driver': 'zarr',\n"
           << "    'kvstore': {'driver': 'file', 'path': out_path},\n"
           << "    'metadata': {\n"
           << "        'dtype': '<f4',\n"
           << "        'shape': [ny, nz, nx],\n"
           << "        'chunks': [ny, min(nz, 64), min(nx, 64)],\n"
           << "    },\n"
           << "    'create': True,\n"
           << "    'delete_existing': True,\n"
           << "}).result()\n"
           << "store[:] = data\n";

    std::string script_path = write_temp_script(script.str());
    std::string cmd = "python3 " + script_path + " " + tmp_bin + " " + path + " 2>&1";
    int ret = std::system(cmd.c_str());  // NOLINT(bugprone-command-processor)

    // Clean up
    std::remove(tmp_bin.c_str());
    std::remove(script_path.c_str());

    if (ret != 0) {
        throw std::runtime_error("TensorStore write failed (Python subprocess)");
    }
}

void TensorStoreFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    // Use Python to read via TensorStore and dump to binary, then load
    const std::string tmp_bin = path + ".tmp_read_bin";

    std::ostringstream script;
    script << "import tensorstore as ts\n"
           << "import numpy as np\n"
           << "import sys\n"
           << "ts_path = sys.argv[1]\n"
           << "out_bin = sys.argv[2]\n"
           << "store = ts.open({\n"
           << "    'driver': 'zarr',\n"
           << "    'kvstore': {'driver': 'file', 'path': ts_path},\n"
           << "    'open': True,\n"
           << "}).result()\n"
           << "data = store.read().result()\n"
           << "np.array(data).astype(np.float32).tofile(out_bin)\n";

    std::string script_path = write_temp_script(script.str());
    std::string cmd = "python3 " + script_path + " " + path + " " + tmp_bin + " 2>&1";
    int ret = std::system(cmd.c_str());  // NOLINT(bugprone-command-processor)

    if (ret != 0) {
        std::remove(tmp_bin.c_str());
        std::remove(script_path.c_str());
        throw std::runtime_error("TensorStore read failed (Python subprocess)");
    }

    // Load the binary output
    {
        std::ifstream f(tmp_bin, std::ios::binary | std::ios::ate);
        if (!f) {
            std::remove(tmp_bin.c_str());
            std::remove(script_path.c_str());
            throw std::runtime_error("TensorStore: cannot read temp binary output");
        }
        auto size = f.tellg();
        if (static_cast<std::size_t>(size) != shape.bytes()) {
            std::remove(tmp_bin.c_str());
            std::remove(script_path.c_str());
            throw std::runtime_error("TensorStore: temp binary size mismatch");
        }
        f.seekg(0);
        f.read(reinterpret_cast<char*>(data), shape.bytes());
    }

    std::remove(tmp_bin.c_str());
    std::remove(script_path.c_str());
}

} // namespace io_bench
