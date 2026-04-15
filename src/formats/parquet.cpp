#include "io_bench/formats.hpp"

#ifdef HAVE_PARQUET
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/writer.h>
#include <parquet/arrow/reader.h>
#endif

#include <stdexcept>

namespace io_bench {

bool ParquetFormat::is_available() const {
#ifdef HAVE_PARQUET
    return true;
#else
    return false;
#endif
}

void ParquetFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
#ifdef HAVE_PARQUET
    // Build a flat table with columns: ix, iy, iz, value
    // This is the standard "long format" for array data in Parquet
    const std::size_t total = shape.total();

    // Create index columns
    arrow::Int32Builder ix_builder;
    arrow::Int32Builder iy_builder;
    arrow::Int32Builder iz_builder;
    arrow::FloatBuilder value_builder;

    auto reserve_status = ix_builder.Reserve(total);
    if (!reserve_status.ok()) { throw std::runtime_error("Parquet reserve failed"); }
    reserve_status = iy_builder.Reserve(total);
    if (!reserve_status.ok()) { throw std::runtime_error("Parquet reserve failed"); }
    reserve_status = iz_builder.Reserve(total);
    if (!reserve_status.ok()) { throw std::runtime_error("Parquet reserve failed"); }
    reserve_status = value_builder.Reserve(total);
    if (!reserve_status.ok()) { throw std::runtime_error("Parquet reserve failed"); }

    for (std::size_t iy = 0; iy < shape.ny; ++iy) {
        for (std::size_t iz = 0; iz < shape.nz; ++iz) {
            for (std::size_t ix = 0; ix < shape.nx; ++ix) {
                ix_builder.UnsafeAppend(static_cast<int32_t>(ix));
                iy_builder.UnsafeAppend(static_cast<int32_t>(iy));
                iz_builder.UnsafeAppend(static_cast<int32_t>(iz));
                value_builder.UnsafeAppend(data[(iy * shape.nz * shape.nx) + (iz * shape.nx) + ix]);
            }
        }
    }

    std::shared_ptr<arrow::Array> ix_array;
    std::shared_ptr<arrow::Array> iy_array;
    std::shared_ptr<arrow::Array> iz_array;
    std::shared_ptr<arrow::Array> value_array;
    auto status = ix_builder.Finish(&ix_array);
    if (!status.ok()) { throw std::runtime_error("Parquet ix build failed"); }
    status = iy_builder.Finish(&iy_array);
    if (!status.ok()) { throw std::runtime_error("Parquet iy build failed"); }
    status = iz_builder.Finish(&iz_array);
    if (!status.ok()) { throw std::runtime_error("Parquet iz build failed"); }
    status = value_builder.Finish(&value_array);
    if (!status.ok()) { throw std::runtime_error("Parquet value build failed"); }

    auto schema = arrow::schema({
        arrow::field("ix", arrow::int32()),
        arrow::field("iy", arrow::int32()),
        arrow::field("iz", arrow::int32()),
        arrow::field("value", arrow::float32())
    });

    auto table = arrow::Table::Make(schema, {ix_array, iy_array, iz_array, value_array});

    // Write Parquet file
    std::shared_ptr<arrow::io::FileOutputStream> outfile;
    auto maybe_outfile = arrow::io::FileOutputStream::Open(path);
    if (!maybe_outfile.ok()) { throw std::runtime_error("Parquet open failed: " + path); }
    outfile = *maybe_outfile;

    auto writer_status = parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, /*chunk_size=*/total);
    if (!writer_status.ok()) { throw std::runtime_error("Parquet write failed"); }

    status = outfile->Close();
    if (!status.ok()) { throw std::runtime_error("Parquet close failed"); }
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("Parquet support not compiled in");
#endif
}

void ParquetFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_PARQUET
    std::shared_ptr<arrow::io::ReadableFile> infile;
    auto maybe_infile = arrow::io::ReadableFile::Open(path);
    if (!maybe_infile.ok()) { throw std::runtime_error("Parquet read open failed: " + path); }
    infile = *maybe_infile;

    std::unique_ptr<parquet::arrow::FileReader> reader;
    auto maybe_reader = parquet::arrow::OpenFile(infile, arrow::default_memory_pool());
    if (!maybe_reader.ok()) { throw std::runtime_error("Parquet reader creation failed"); }
    reader = std::move(*maybe_reader);

    std::shared_ptr<arrow::Table> table;
    auto read_status = reader->ReadTable(&table);
    if (!read_status.ok()) { throw std::runtime_error("Parquet read failed"); }

    // Validate row count matches expected shape
    if (static_cast<std::size_t>(table->num_rows()) != shape.total()) {
        throw std::runtime_error("Parquet row count mismatch: expected " +
            std::to_string(shape.total()) + ", got " + std::to_string(table->num_rows()));
    }

    // Extract value column (column 3)
    auto value_chunked = table->column(3);
    const auto& chunks = value_chunked->chunks();
    std::size_t idx = 0;
    for (const auto& chunk : chunks) {
        auto float_array = std::static_pointer_cast<arrow::FloatArray>(chunk);
        for (int64_t i = 0; i < float_array->length(); ++i) {
            data[idx++] = float_array->Value(i);
        }
    }
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("Parquet support not compiled in");
#endif
}

} // namespace io_bench
