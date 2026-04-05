#include "io_bench/formats.hpp"
#include <stdexcept>

#ifdef HAVE_TILEDB
#include <tiledb/tiledb>
#endif

namespace io_bench {

bool TileDBFormat::is_available() const {
#ifdef HAVE_TILEDB
    return true;
#else
    return false;
#endif
}

void TileDBFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
#ifdef HAVE_TILEDB
    using namespace tiledb;
    
    Context ctx;
    
    // Create array schema
    ArraySchema schema(ctx, TILEDB_DENSE);
    
    Domain domain(ctx);
    domain.add_dim(Dimension::create<uint64_t>(ctx, "z", {0, shape.nz - 1}));
    domain.add_dim(Dimension::create<uint64_t>(ctx, "x", {0, shape.nx - 1}));
    schema.set_domain(domain);
    
    Attribute attr = Attribute::create<float>(ctx, "velocity");
    schema.add_attribute(attr);
    
    Array::create(path, schema);
    
    // Write data
    Array array(ctx, path, TILEDB_WRITE);
    Query query(ctx, array);
    query.set_data_buffer("velocity", const_cast<float*>(data), shape.total());
    query.submit();
    array.close();
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("TileDB support not compiled in");
#endif
}

void TileDBFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_TILEDB
    using namespace tiledb;
    
    Context ctx;
    Array array(ctx, path, TILEDB_READ);
    
    Query query(ctx, array);
    query.set_data_buffer("velocity", data, shape.total());
    query.submit();
    array.close();
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("TileDB support not compiled in");
#endif
}

} // namespace io_bench
