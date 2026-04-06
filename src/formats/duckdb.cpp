#include "io_bench/formats.hpp"
#include <stdexcept>
#include <fstream>
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
    duckdb_database db = nullptr;
    duckdb_connection conn = nullptr;
    duckdb_result result;
    
    // Open in-memory database then save to file
    std::string conn_str = path;
    
    duckdb_state status = duckdb_open(conn_str.c_str(), &db);
    if (status != DuckDBSuccess || db == nullptr) {
        throw std::runtime_error("DuckDB: Failed to open database: " + path);
    }
    
    status = duckdb_connect(db, &conn);
    if (status != DuckDBSuccess) {
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to connect");
    }
    
    // Create table with appropriate dimensions
    std::ostringstream create_sql;
    create_sql << "CREATE TABLE velocity (";
    if (shape.ny > 1) {
        create_sql << "ix INTEGER, iy INTEGER, iz INTEGER, value FLOAT";
    } else {
        create_sql << "ix INTEGER, iz INTEGER, value FLOAT";
    }
    create_sql << ")";
    
    status = duckdb_query(conn, create_sql.str().c_str(), &result);
    if (status != DuckDBSuccess) {
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to create table");
    }
    duckdb_destroy_result(&result);
    
    // Insert data
    std::ostringstream insert_sql;
    insert_sql << "INSERT INTO velocity VALUES ";
    
    bool first = true;
    for (std::size_t iz = 0; iz < shape.nz; ++iz) {
        for (std::size_t iy = 0; iy < shape.ny; ++iy) {
            for (std::size_t ix = 0; ix < shape.nx; ++ix) {
                std::size_t idx;
                if (shape.ny > 1) {
                    idx = iz * shape.ny * shape.nx + iy * shape.nx + ix;
                } else {
                    idx = iz * shape.nx + ix;
                }
                
                if (!first) insert_sql << ", ";
                first = false;
                
                if (shape.ny > 1) {
                    insert_sql << "(" << ix << ", " << iy << ", " << iz << ", " << data[idx] << ")";
                } else {
                    insert_sql << "(" << ix << ", " << iz << ", " << data[idx] << ")";
                }
            }
        }
    }
    
    status = duckdb_query(conn, insert_sql.str().c_str(), &result);
    if (status != DuckDBSuccess) {
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to insert data");
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

void DuckDBFormat::read(const std::string& path, float* data, const ArrayShape& shape) {
#ifdef HAVE_DUCKDB
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
    
    // Read data
    status = duckdb_query(conn, "SELECT * FROM velocity ORDER BY iz, iy, ix", &result);
    if (status != DuckDBSuccess) {
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        throw std::runtime_error("DuckDB: Failed to query data");
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
    
    // Extract values
    for (std::size_t i = 0; i < rows; ++i) {
        // Value is in the last column (use duckdb_value_float directly)
        // Column index: shape.ny > 1 ? 3 : 2 (last column is value)
        int col_idx = shape.ny > 1 ? 3 : 2;
        
        // Get value as double and convert to float (DuckDB returns DOUBLE for FLOAT columns)
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

} // namespace io_bench
