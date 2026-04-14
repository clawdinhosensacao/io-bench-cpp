#include "io_bench/formats.hpp"
#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <sstream>

#ifdef HAVE_DUCKDB
#include <duckdb.h>
#endif

namespace io_bench {

bool DuckDBFormat::is_available() const {
#ifdef HAVE_DUCKDB
    return true;
#else
    return false;
#endif
}

void DuckDBFormat::write(const std::string& path, const float* data, const ArrayShape& shape) {
#ifdef HAVE_DUCKDB
    // Remove existing database file/directory (DuckDB creates a directory on disk)
    std::filesystem::remove_all(path);

    duckdb_database db = nullptr;
    duckdb_connection conn = nullptr;
    duckdb_result result;

    duckdb_state status = duckdb_open(path.c_str(), &db);
    if (status != DuckDBSuccess || db == nullptr) {
        throw std::runtime_error("DuckDB: Failed to open database: " + path);
    }

    status = duckdb_connect(db, &conn);
    if (status != DuckDBSuccess) {
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to connect");
    }
    std::ostringstream create_sql;
    create_sql << "CREATE TABLE velocity (";
    if (shape.is_3d()) {
        create_sql << "ix INTEGER, iy INTEGER, iz INTEGER, value FLOAT";
    } else {
        create_sql << "ix INTEGER, iz INTEGER, value FLOAT";
    }
    create_sql << ")";

    status = duckdb_query(conn, create_sql.str().c_str(), &result);
    if (status != DuckDBSuccess) {
        const char* err = duckdb_result_error(&result);
        std::string msg = err ? err : "unknown error";
        duckdb_destroy_result(&result);
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to create table: " + msg);
    }
    duckdb_destroy_result(&result);

    // Use Appender API for efficient bulk insert with exact float precision
    duckdb_appender appender = nullptr;
    status = duckdb_appender_create(conn, "main", "velocity", &appender);
    if (status != DuckDBSuccess) {
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to create appender");
    }

    for (std::size_t iz = 0; iz < shape.nz; ++iz) {
        for (std::size_t iy = 0; iy < shape.ny; ++iy) {
            for (std::size_t ix = 0; ix < shape.nx; ++ix) {
                std::size_t idx;
                if (shape.is_3d()) {
                    idx = iz * shape.ny * shape.nx + iy * shape.nx + ix;
                } else {
                    idx = iz * shape.nx + ix;
                }

                duckdb_append_int32(appender, static_cast<int32_t>(ix));
                if (shape.is_3d()) {
                    duckdb_append_int32(appender, static_cast<int32_t>(iy));
                }
                duckdb_append_int32(appender, static_cast<int32_t>(iz));
                float fval = data[idx];
                duckdb_append_float(appender, fval);

                status = duckdb_appender_end_row(appender);
                if (status != DuckDBSuccess) {
                    duckdb_appender_destroy(&appender);
                    duckdb_disconnect(&conn);
                    duckdb_close(&db);
                    throw std::runtime_error("DuckDB: Failed to append row");
                }
            }
        }
    }

    status = duckdb_appender_flush(appender);
    if (status != DuckDBSuccess) {
        duckdb_appender_destroy(&appender);
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to flush appender");
    }

    duckdb_appender_destroy(&appender);
    duckdb_disconnect(&conn);
    duckdb_close(&db);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("DuckDB support not compiled in");
#endif
}

void DuckDBFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_DUCKDB
    duckdb_database db = nullptr;
    duckdb_connection conn = nullptr;
    duckdb_result result;

    // Open read-only — avoids write-lock contention and is semantically correct
    duckdb_config config = nullptr;
    char* config_err = nullptr;
    duckdb_state cfg_status = duckdb_create_config(&config);
    if (cfg_status == DuckDBSuccess) {
        cfg_status = duckdb_set_config(config, "access_mode", "READ_ONLY");
    }
    duckdb_state status;
    if (cfg_status == DuckDBSuccess) {
        status = duckdb_open_ext(path.c_str(), &db, config, &config_err);
    } else {
        status = duckdb_open(path.c_str(), &db);
    }
    if (config) { duckdb_destroy_config(&config); }
    if (status != DuckDBSuccess || db == nullptr) {
        throw std::runtime_error("DuckDB: Failed to open database: " + path);
    }

    status = duckdb_connect(db, &conn);
    if (status != DuckDBSuccess) {
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to connect");
    }

    // Read data — ORDER BY must match the column schema (2D has no iy column)
    std::string select_sql = shape.is_3d()
        ? "SELECT * FROM velocity ORDER BY iz, iy, ix"
        : "SELECT * FROM velocity ORDER BY iz, ix";
    status = duckdb_query(conn, select_sql.c_str(), &result);
    if (status != DuckDBSuccess) {
        const char* err = duckdb_result_error(&result);
        std::string msg = err ? err : "unknown error";
        duckdb_destroy_result(&result);
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to query data: " + msg);
    }

    // Get row count
    std::size_t rows = duckdb_row_count(&result);
    std::size_t expected = shape.nx * shape.nz * shape.ny;

    if (rows != expected) {
        duckdb_destroy_result(&result);
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Row count mismatch");
    }

    // Extract values from the value column (last column)
    const int col_idx = shape.is_3d() ? 3 : 2;
    for (std::size_t i = 0; i < rows; ++i) {
        double val = duckdb_value_double(&result, col_idx, static_cast<idx_t>(i));
        data[i] = static_cast<float>(val);
    }

    duckdb_destroy_result(&result);
    duckdb_disconnect(&conn);
    duckdb_close(&db);
#else
    (void)path;
    (void)data;
    (void)shape;
    throw std::runtime_error("DuckDB support not compiled in");
#endif
}

void DuckDBFormat::read_slice(const std::string& path, float* slice_buf,
                                const ArrayShape& shape, std::size_t iy) {
#ifdef HAVE_DUCKDB
    duckdb_database db = nullptr;
    duckdb_connection conn = nullptr;
    duckdb_result result;

    // Open read-only
    duckdb_config config = nullptr;
    char* config_err = nullptr;
    duckdb_state cfg_status = duckdb_create_config(&config);
    if (cfg_status == DuckDBSuccess) {
        cfg_status = duckdb_set_config(config, "access_mode", "READ_ONLY");
    }
    duckdb_state status;
    if (cfg_status == DuckDBSuccess) {
        status = duckdb_open_ext(path.c_str(), &db, config, &config_err);
    } else {
        status = duckdb_open(path.c_str(), &db);
    }
    if (config) { duckdb_destroy_config(&config); }
    if (status != DuckDBSuccess || db == nullptr) {
        throw std::runtime_error("DuckDB: Failed to open database: " + path);
    }

    status = duckdb_connect(db, &conn);
    if (status != DuckDBSuccess) {
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to connect");
    }

    // Select only the iy-th inline — avoids reading the entire volume
    // Result rows come in ORDER BY iz, ix — one row per (iz, ix) pair.
    // We need to map back to the in-memory slice layout: slice_buf[iz * nx + ix]
    std::ostringstream sql;
    sql << "SELECT iz, ix, value FROM velocity WHERE iy = " << iy
        << " ORDER BY iz, ix";
    status = duckdb_query(conn, sql.str().c_str(), &result);
    if (status != DuckDBSuccess) {
        const char* err = duckdb_result_error(&result);
        std::string msg = err ? err : "unknown error";
        duckdb_destroy_result(&result);
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to query slice: " + msg);
    }

    std::size_t rows = duckdb_row_count(&result);
    std::size_t expected = shape.nx * shape.nz;
    if (rows != expected) {
        duckdb_destroy_result(&result);
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Slice row count mismatch");
    }

    // Map SQL rows (iz, ix, value) → slice_buf[iz * nx + ix]
    for (std::size_t i = 0; i < rows; ++i) {
        auto iz_val = static_cast<std::size_t>(duckdb_value_int32(&result, 0, static_cast<idx_t>(i)));
        auto ix_val = static_cast<std::size_t>(duckdb_value_int32(&result, 1, static_cast<idx_t>(i)));
        double val = duckdb_value_double(&result, 2, static_cast<idx_t>(i));
        std::size_t out_idx = iz_val * shape.nx + ix_val;
        slice_buf[out_idx] = static_cast<float>(val);
    }

    duckdb_destroy_result(&result);
    duckdb_disconnect(&conn);
    duckdb_close(&db);
#else
    (void)path;
    (void)slice_buf;
    (void)shape;
    (void)iy;
    throw std::runtime_error("DuckDB support not compiled in");
#endif
}

} // namespace io_bench
