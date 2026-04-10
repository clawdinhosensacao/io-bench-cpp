#include "io_bench/formats.hpp"

#ifdef HAVE_TENSORSTORE_CPP

#include <tensorstore/tensorstore.h>
#include <tensorstore/open.h>
#include <tensorstore/array.h>
#include <tensorstore/index_space.h>
#include <tensorstore/util/result.h>
#include <tensorstore/util/status.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace io_bench {

std::string TensorStoreFormat::name() const {
    return "tensorstore_cpp";  // Distinguish from Python bridge
}

bool TensorStoreFormat::is_available() const {
    return true;  // Native C++ API, always available when compiled with HAVE_TENSORSTORE_CPP
}

void TensorStoreFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    // Remove existing directory if present
    if (std::filesystem::exists(path)) {
        std::filesystem::remove_all(path);
    }

    // Open TensorStore for writing (zarr driver, file kvstore)
    auto open_result = tensorstore::Open({
        {"driver", "zarr"},
        {"kvstore", {{"driver", "file"}, {"path", path}}},
        {"metadata", {
            {"dtype", "<f4"},
            {"shape", {static_cast<tensorstore::Index>(shape.ny),
                       static_cast<tensorstore::Index>(shape.nz),
                       static_cast<tensorstore::Index>(shape.nx)}},
            {"chunks", {static_cast<tensorstore::Index>(std::min(shape.ny, std::size_t{64})),
                        static_cast<tensorstore::Index>(std::min(shape.nz, std::size_t{64})),
                        static_cast<tensorstore::Index>(std::min(shape.nx, std::size_t{64}))}},
            {"order", "C"},
            {"compressor", nullptr},  // No compression for benchmark consistency
        }},
        {"create", true},
        {"delete_existing", true},
    }).result();

    if (!open_result.ok()) {
        throw std::runtime_error("TensorStore native: open for write failed: " +
                                  open_result.status().ToString());
    }

    auto store = *open_result;

    // Create an array from the raw data pointer
    auto array = tensorstore::AllocateArray<float>(
        {static_cast<tensorstore::Index>(shape.ny),
         static_cast<tensorstore::Index>(shape.nz),
         static_cast<tensorstore::Index>(shape.nx)},
        tensorstore::c_order, tensorstore::value_init);

    std::memcpy(array.data(), data, shape.bytes());

    // Write the array to the store
    auto write_result = tensorstore::Write(array, store).result();
    if (!write_result.ok()) {
        throw std::runtime_error("TensorStore native: write failed: " +
                                  write_result.status().ToString());
    }
}

void TensorStoreFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
    // Open TensorStore for reading
    auto open_result = tensorstore::Open({
        {"driver", "zarr"},
        {"kvstore", {{"driver", "file"}, {"path", path}}},
        {"open", true},
    }).result();

    if (!open_result.ok()) {
        throw std::runtime_error("TensorStore native: open for read failed: " +
                                  open_result.status().ToString());
    }

    auto store = *open_result;

    // Read the entire array
    auto read_result = tensorstore::Read(store).result();
    if (!read_result.ok()) {
        throw std::runtime_error("TensorStore native: read failed: " +
                                  read_result.status().ToString());
    }

    // Copy data to output buffer
    const auto& array = *read_result;
    std::memcpy(data, array.byte_strided_origin_pointer().get(), shape.bytes());
}

} // namespace io_bench

#else // !HAVE_TENSORSTORE_CPP

// Fallback: Python bridge implementation (when TensorStore C++ is not available)

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace io_bench {

std::string TensorStoreFormat::name() const {
    return "tensorstore";  // Python bridge version
}

bool TensorStoreFormat::is_available() const {
    // Check if the Python tensorstore module is importable
    int ret = std::system("python3 -c 'import tensorstore' 2>/dev/null");  // NOLINT(bugprone-command-processor)
    return ret == 0;
}

static std::string write_temp_script(const std::string& content) {
    std::string tmp_path = std::filesystem::temp_directory_path() / "ts_bench_write.py";
    std::ofstream f(tmp_path);
    if (!f) { throw std::runtime_error("TensorStore: cannot create temp script"); }
    f << content;
    f.close();
    return tmp_path;
}

void TensorStoreFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
    const std::string tmp_bin = path + ".tmp_bin";

    {
        std::ofstream f(tmp_bin, std::ios::binary);
        if (!f) { throw std::runtime_error("TensorStore: cannot write temp binary"); }
        f.write(reinterpret_cast<const char*>(data), shape.bytes());
    }

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
           << "        'chunks': [min(ny, 64), min(nz, 64), min(nx, 64)],\n"
           << "    },\n"
           << "    'create': True,\n"
           << "    'delete_existing': True,\n"
           << "}).result()\n"
           << "store[:] = data\n";

    std::string script_path = write_temp_script(script.str());
    std::string cmd = "python3 " + script_path + " " + tmp_bin + " " + path + " 2>&1";
    int ret = std::system(cmd.c_str());  // NOLINT(bugprone-command-processor)

    std::remove(tmp_bin.c_str());
    std::remove(script_path.c_str());

    if (ret != 0) {
        throw std::runtime_error("TensorStore write failed (Python subprocess)");
    }
}

void TensorStoreFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
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

#endif // HAVE_TENSORSTORE_CPP
