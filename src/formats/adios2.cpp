#include "io_bench/formats.hpp"
#include <stdexcept>
#include <cstring>

#ifdef HAVE_ADIOS2
#include <adios2_c.h>
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
    adios2_adios* adios = adios2_init(ADIOS2_DEBUG_MODE_ON);
    
    adios2_io* io = adios2_declare_io(adios, "io_bench");
    
    // Define variable
    size_t dims[2] = {shape.nz, shape.nx};
    adios2_variable* var = adios2_define_variable(io, "velocity", adios2_type_float,
                                                   2, dims, adios2_constant_dims,
                                                   nullptr, nullptr, nullptr);
    
    // Open engine and write
    adios2_engine* engine = adios2_open(io, path.c_str(), adios2_mode_write);
    
    adios2_put(engine, var, data, adios2_mode_deferred);
    
    adios2_close(engine);
    adios2_finalize(adios);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("ADIOS2 support not compiled in");
#endif
}

void Adios2Format::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_ADIOS2
    adios2_adios* adios = adios2_init(ADIOS2_DEBUG_MODE_ON);
    
    adios2_io* io = adios2_declare_io(adios, "io_bench");
    
    adios2_engine* engine = adios2_open(io, path.c_str(), adios2_mode_read);
    
    adios2_variable* var = adios2_inquire_variable(io, "velocity");
    if (!var) {
        adios2_close(engine);
        adios2_finalize(adios);
        throw std::runtime_error("ADIOS2 variable 'velocity' not found");
    }
    
    // Check shape
    size_t dims[2];
    adios2_variable_shape(var, dims);
    if (dims[0] != shape.nz || dims[1] != shape.nx) {
        adios2_close(engine);
        adios2_finalize(adios);
        throw std::runtime_error("ADIOS2 variable shape mismatch");
    }
    
    adios2_get(engine, var, data, adios2_mode_sync);
    
    adios2_close(engine);
    adios2_finalize(adios);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("ADIOS2 support not compiled in");
#endif
}

} // namespace io_bench
