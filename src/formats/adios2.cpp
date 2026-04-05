#include "io_bench/formats.hpp"
#include <stdexcept>

#ifdef HAVE_ADIOS2
#include <adios2.h>
#endif

namespace io_bench {

bool Adios2Format::is_available() const {
#ifdef HAVE_ADIOS2
    return true;
#else
    return false;
#endif
}

void Adios2Format::write(const std::string& path, const float* data, const ArrayShape& shape) {
#ifdef HAVE_ADIOS2
    adios2::ADIOS adios;
    adios2::IO io = adios.DeclareIO("writer");
    
    // Define variable
    adios2::Dims dims = {shape.nz, shape.nx};
    auto var = io.DefineVariable<float>("velocity", dims);
    
    // Open file and write
    adios2::Engine engine = io.Open(path, adios2::Mode::Write);
    engine.Put(var, data);
    engine.Close();
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("ADIOS2 support not compiled in");
#endif
}

void Adios2Format::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_ADIOS2
    adios2::ADIOS adios;
    adios2::IO io = adios.DeclareIO("reader");
    
    adios2::Engine engine = io.Open(path, adios2::Mode::Read);
    
    // Get variable info
    auto var = io.InquireVariable<float>("velocity");
    if (!var) {
        throw std::runtime_error("ADIOS2: variable 'velocity' not found");
    }
    
    auto dims = var.Shape();
    if (dims.size() != 2 || dims[0] != shape.nz || dims[1] != shape.nx) {
        throw std::runtime_error("ADIOS2: shape mismatch");
    }
    
    engine.Get(var, data);
    engine.Close();
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("ADIOS2 support not compiled in");
#endif
}

} // namespace io_bench
