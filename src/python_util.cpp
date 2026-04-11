#include "io_bench/types.hpp"
#include <cstdlib>

namespace io_bench {

std::string find_python_with_module(const char* module_name) {
    // Try python3.13 first, then python3
    const char* py_candidates[] = {"python3.13", "python3"};
    for (const char* py : py_candidates) {
        std::string cmd = std::string(py) + " -c 'import " + module_name + "' 2>/dev/null";
        int ret = std::system(cmd.c_str());  // NOLINT(bugprone-command-processor)
        if (ret == 0) {
            return py;
        }
    }
    return "";
}

} // namespace io_bench
