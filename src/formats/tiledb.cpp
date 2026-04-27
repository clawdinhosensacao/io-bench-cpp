#include "io_bench/formats.hpp"
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <filesystem>

#ifdef HAVE_TILEDB
#include <tiledb/tiledb.h>
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
    tiledb_ctx_t* ctx = nullptr;
    tiledb_ctx_alloc(nullptr, &ctx);

    // Remove existing array directory if it exists
    std::filesystem::remove_all(path);

    // Create dimensions: y (slowest), z, x (fastest)
    tiledb_dimension_t* x_dim = nullptr;
    int64_t x_ext[] = {0, static_cast<int64_t>(shape.nx - 1)};
    tiledb_dimension_alloc(ctx, "x", TILEDB_INT64, x_ext, nullptr, &x_dim);

    tiledb_dimension_t* z_dim = nullptr;
    int64_t z_ext[] = {0, static_cast<int64_t>(shape.nz - 1)};
    tiledb_dimension_alloc(ctx, "z", TILEDB_INT64, z_ext, nullptr, &z_dim);

    tiledb_dimension_t* y_dim = nullptr;
    tiledb_dimension_t* domain_y = nullptr;
    if (shape.is_3d()) {
        int64_t y_ext[] = {0, static_cast<int64_t>(shape.ny - 1)};
        tiledb_dimension_alloc(ctx, "y", TILEDB_INT64, y_ext, nullptr, &y_dim);
        domain_y = y_dim;
    }

    // Create domain: add dimensions in row-major order (slowest to fastest)
    tiledb_domain_t* domain = nullptr;
    tiledb_domain_alloc(ctx, &domain);
    if (domain_y != nullptr) {
        tiledb_domain_add_dimension(ctx, domain, domain_y);
    }
    tiledb_domain_add_dimension(ctx, domain, z_dim);
    tiledb_domain_add_dimension(ctx, domain, x_dim);

    // Create attribute
    tiledb_attribute_t* attr = nullptr;
    tiledb_attribute_alloc(ctx, "velocity", TILEDB_FLOAT32, &attr);

    // Create schema
    tiledb_array_schema_t* schema = nullptr;
    tiledb_array_schema_alloc(ctx, TILEDB_DENSE, &schema);
    tiledb_array_schema_set_domain(ctx, schema, domain);
    tiledb_array_schema_add_attribute(ctx, schema, attr);

    // Create array
    tiledb_array_create(ctx, path.c_str(), schema);

    // Open array for writing
    tiledb_array_t* array = nullptr;
    tiledb_array_alloc(ctx, path.c_str(), &array);
    tiledb_array_open(ctx, array, TILEDB_WRITE);

    // Create subarray for dense write
    tiledb_subarray_t* subarray = nullptr;
    tiledb_subarray_alloc(ctx, array, &subarray);

    int64_t y_start = 0;
    auto y_end = static_cast<int64_t>(shape.ny - 1);
    int64_t z_start = 0;
    auto z_end = static_cast<int64_t>(shape.nz - 1);
    int64_t x_start = 0;
    auto x_end = static_cast<int64_t>(shape.nx - 1);

    int dim_idx = 0;
    if (shape.is_3d()) {
        tiledb_subarray_add_range(ctx, subarray, dim_idx, &y_start, &y_end, nullptr);
        ++dim_idx;
    }
    tiledb_subarray_add_range(ctx, subarray, dim_idx, &z_start, &z_end, nullptr);
    ++dim_idx;
    tiledb_subarray_add_range(ctx, subarray, dim_idx, &x_start, &x_end, nullptr);

    // Create query
    tiledb_query_t* query = nullptr;
    tiledb_query_alloc(ctx, array, TILEDB_WRITE, &query);
    tiledb_query_set_layout(ctx, query, TILEDB_ROW_MAJOR);
    tiledb_query_set_subarray_t(ctx, query, subarray);

    // Set data buffer
    uint64_t size = shape.total() * sizeof(float);
    tiledb_query_set_data_buffer(ctx, query, "velocity", const_cast<void*>(static_cast<const void*>(data)), &size);

    // Submit query
    tiledb_query_submit(ctx, query);
    tiledb_query_finalize(ctx, query);

    // Close array
    tiledb_array_close(ctx, array);

    // Cleanup
    tiledb_subarray_free(&subarray);
    tiledb_query_free(&query);
    tiledb_array_free(&array);
    tiledb_array_schema_free(&schema);
    tiledb_attribute_free(&attr);
    tiledb_domain_free(&domain);
    if (y_dim != nullptr) { tiledb_dimension_free(&y_dim); }
    tiledb_dimension_free(&z_dim);
    tiledb_dimension_free(&x_dim);
    tiledb_ctx_free(&ctx);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("TileDB support not compiled in");
#endif
}

void TileDBFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_TILEDB
    tiledb_ctx_t* ctx = nullptr;
    tiledb_ctx_alloc(nullptr, &ctx);

    // Open array for reading
    tiledb_array_t* array = nullptr;
    tiledb_array_alloc(ctx, path.c_str(), &array);
    tiledb_array_open(ctx, array, TILEDB_READ);

    // Create subarray for dense read
    tiledb_subarray_t* subarray = nullptr;
    tiledb_subarray_alloc(ctx, array, &subarray);

    int64_t y_start = 0;
    auto y_end = static_cast<int64_t>(shape.ny - 1);
    int64_t z_start = 0;
    auto z_end = static_cast<int64_t>(shape.nz - 1);
    int64_t x_start = 0;
    auto x_end = static_cast<int64_t>(shape.nx - 1);

    int dim_idx = 0;
    if (shape.is_3d()) {
        tiledb_subarray_add_range(ctx, subarray, dim_idx, &y_start, &y_end, nullptr);
        ++dim_idx;
    }
    tiledb_subarray_add_range(ctx, subarray, dim_idx, &z_start, &z_end, nullptr);
    ++dim_idx;
    tiledb_subarray_add_range(ctx, subarray, dim_idx, &x_start, &x_end, nullptr);

    // Create query
    tiledb_query_t* query = nullptr;
    tiledb_query_alloc(ctx, array, TILEDB_READ, &query);
    tiledb_query_set_layout(ctx, query, TILEDB_ROW_MAJOR);
    tiledb_query_set_subarray_t(ctx, query, subarray);

    // Set data buffer
    uint64_t size = shape.total() * sizeof(float);
    tiledb_query_set_data_buffer(ctx, query, "velocity", data, &size);

    // Submit query
    tiledb_query_submit(ctx, query);
    tiledb_query_finalize(ctx, query);

    // Close array
    tiledb_array_close(ctx, array);

    // Cleanup
    tiledb_subarray_free(&subarray);
    tiledb_query_free(&query);
    tiledb_array_free(&array);
    tiledb_ctx_free(&ctx);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("TileDB support not compiled in");
#endif
}

void TileDBFormat::read_slice(const std::string& path, float* slice_buf,
                                const ArrayShape& shape, std::size_t iy) {
#ifdef HAVE_TILEDB
    tiledb_ctx_t* ctx = nullptr;
    tiledb_ctx_alloc(nullptr, &ctx);

    // Open array for reading
    tiledb_array_t* array = nullptr;
    tiledb_array_alloc(ctx, path.c_str(), &array);
    tiledb_array_open(ctx, array, TILEDB_READ);

    // Create subarray selecting only one inline (iy-th slice)
    // Domain layout: y (slowest), z, x (fastest)
    tiledb_subarray_t* subarray = nullptr;
    tiledb_subarray_alloc(ctx, array, &subarray);

    auto iy_val = static_cast<int64_t>(iy);
    int64_t z_start = 0;
    auto z_end = static_cast<int64_t>(shape.nz - 1);
    int64_t x_start = 0;
    auto x_end = static_cast<int64_t>(shape.nx - 1);

    // Dimension 0 = y, Dimension 1 = z, Dimension 2 = x
    tiledb_subarray_add_range(ctx, subarray, 0, &iy_val, &iy_val, nullptr);
    tiledb_subarray_add_range(ctx, subarray, 1, &z_start, &z_end, nullptr);
    tiledb_subarray_add_range(ctx, subarray, 2, &x_start, &x_end, nullptr);

    // Create query
    tiledb_query_t* query = nullptr;
    tiledb_query_alloc(ctx, array, TILEDB_READ, &query);
    tiledb_query_set_layout(ctx, query, TILEDB_ROW_MAJOR);
    tiledb_query_set_subarray_t(ctx, query, subarray);

    // Set data buffer — only nx * nz elements for one inline
    uint64_t size = static_cast<uint64_t>(shape.nx * shape.nz) * sizeof(float);
    tiledb_query_set_data_buffer(ctx, query, "velocity", slice_buf, &size);

    // Submit query
    tiledb_query_submit(ctx, query);
    tiledb_query_finalize(ctx, query);

    // Close array
    tiledb_array_close(ctx, array);

    // Cleanup
    tiledb_subarray_free(&subarray);
    tiledb_query_free(&query);
    tiledb_array_free(&array);
    tiledb_ctx_free(&ctx);
#else
    (void)path;
    (void)slice_buf;
    (void)shape;
    (void)iy;
    throw std::runtime_error("TileDB support not compiled in");
#endif
}

void TileDBFormat::write_compressed(const std::string& path, const float* data,
                                      const ArrayShape& shape, int level) {
#ifdef HAVE_TILEDB
    tiledb_ctx_t* ctx = nullptr;
    tiledb_ctx_alloc(nullptr, &ctx);

    // Remove existing array directory if it exists
    std::filesystem::remove_all(path);

    // Create dimensions
    tiledb_dimension_t* x_dim = nullptr;
    int64_t x_ext[] = {0, static_cast<int64_t>(shape.nx - 1)};
    tiledb_dimension_alloc(ctx, "x", TILEDB_INT64, x_ext, nullptr, &x_dim);

    tiledb_dimension_t* z_dim = nullptr;
    int64_t z_ext[] = {0, static_cast<int64_t>(shape.nz - 1)};
    tiledb_dimension_alloc(ctx, "z", TILEDB_INT64, z_ext, nullptr, &z_dim);

    tiledb_dimension_t* y_dim = nullptr;
    if (shape.is_3d()) {
        int64_t y_ext[] = {0, static_cast<int64_t>(shape.ny - 1)};
        tiledb_dimension_alloc(ctx, "y", TILEDB_INT64, y_ext, nullptr, &y_dim);
    }

    // Create domain
    tiledb_domain_t* domain = nullptr;
    tiledb_domain_alloc(ctx, &domain);
    if (y_dim != nullptr) {
        tiledb_domain_add_dimension(ctx, domain, y_dim);
    }
    tiledb_domain_add_dimension(ctx, domain, z_dim);
    tiledb_domain_add_dimension(ctx, domain, x_dim);

    // Create attribute with compression filter
    tiledb_attribute_t* attr = nullptr;
    tiledb_attribute_alloc(ctx, "velocity", TILEDB_FLOAT32, &attr);

    if (level > 0) {
        tiledb_filter_t* filter = nullptr;
        tiledb_filter_alloc(ctx, TILEDB_FILTER_ZSTD, &filter);
        tiledb_filter_set_option(ctx, filter, TILEDB_COMPRESSION_LEVEL, &level);
        tiledb_filter_list_t* filters = nullptr;
        tiledb_filter_list_alloc(ctx, &filters);
        tiledb_filter_list_add_filter(ctx, filters, filter);
        tiledb_attribute_set_filter_list(ctx, attr, filters);
        tiledb_filter_list_free(&filters);
        tiledb_filter_free(&filter);
    }

    // Create schema
    tiledb_array_schema_t* schema = nullptr;
    tiledb_array_schema_alloc(ctx, TILEDB_DENSE, &schema);
    tiledb_array_schema_set_domain(ctx, schema, domain);
    tiledb_array_schema_add_attribute(ctx, schema, attr);

    // Create array
    tiledb_array_create(ctx, path.c_str(), schema);

    // Open array for writing
    tiledb_array_t* array = nullptr;
    tiledb_array_alloc(ctx, path.c_str(), &array);
    tiledb_array_open(ctx, array, TILEDB_WRITE);

    // Create subarray
    tiledb_subarray_t* subarray = nullptr;
    tiledb_subarray_alloc(ctx, array, &subarray);

    int64_t y_start = 0;
    auto y_end = static_cast<int64_t>(shape.ny - 1);
    int64_t z_start = 0;
    auto z_end = static_cast<int64_t>(shape.nz - 1);
    int64_t x_start = 0;
    auto x_end = static_cast<int64_t>(shape.nx - 1);

    int dim_idx = 0;
    if (shape.is_3d()) {
        tiledb_subarray_add_range(ctx, subarray, dim_idx, &y_start, &y_end, nullptr);
        ++dim_idx;
    }
    tiledb_subarray_add_range(ctx, subarray, dim_idx, &z_start, &z_end, nullptr);
    ++dim_idx;
    tiledb_subarray_add_range(ctx, subarray, dim_idx, &x_start, &x_end, nullptr);

    // Create query
    tiledb_query_t* query = nullptr;
    tiledb_query_alloc(ctx, array, TILEDB_WRITE, &query);
    tiledb_query_set_layout(ctx, query, TILEDB_ROW_MAJOR);
    tiledb_query_set_subarray_t(ctx, query, subarray);

    uint64_t size = shape.total() * sizeof(float);
    tiledb_query_set_data_buffer(ctx, query, "velocity", const_cast<void*>(static_cast<const void*>(data)), &size);

    tiledb_query_submit(ctx, query);
    tiledb_query_finalize(ctx, query);
    tiledb_array_close(ctx, array);

    // Cleanup
    tiledb_subarray_free(&subarray);
    tiledb_query_free(&query);
    tiledb_array_free(&array);
    tiledb_array_schema_free(&schema);
    tiledb_attribute_free(&attr);
    tiledb_domain_free(&domain);
    if (y_dim != nullptr) { tiledb_dimension_free(&y_dim); }
    tiledb_dimension_free(&z_dim);
    tiledb_dimension_free(&x_dim);
    tiledb_ctx_free(&ctx);
#else
    (void)path;
    (void)data;
    (void)shape;
    (void)level;
    throw std::runtime_error("TileDB support not compiled in");
#endif
}

} // namespace io_bench
