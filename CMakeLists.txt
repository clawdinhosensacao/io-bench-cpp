cmake_minimum_required(VERSION 3.16)
project(io_bench_cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Dependencies (find packages, some optional)
find_package(HDF5 QUIET COMPONENTS CXX)
find_package(NetCDF QUIET)
find_package(Parquet QUIET)  # Apache Arrow
find_package(TileDB QUIET)
find_package(adios2 QUIET)
find_package(DuckDB QUIET)

# FetchContent for dependencies we need to download
include(FetchContent)

# nlohmann/json (required)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# HighFive HDF5 wrapper (optional, if HDF5 found)
if(HDF5_FOUND)
    FetchContent_Declare(
        highfive
        GIT_REPOSITORY https://github.com/BlueBrain/HighFive.git
        GIT_TAG v2.9.0
    )
    set(HIGHFIVE_USE_BOOST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(highfive)
endif()

# cnpy for NPY format
FetchContent_Declare(
    cnpy
    GIT_REPOSITORY https://github.com/rogersce/cnpy.git
    GIT_TAG master
)
FetchContent_MakeAvailable(cnpy)

# Google Benchmark
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

# Main library sources
add_library(io_bench_lib STATIC
    src/benchmark.cpp
    src/formats/binary.cpp
    src/formats/npy.cpp
    src/formats/json.cpp
    src/report.cpp
)

target_include_directories(io_bench_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(io_bench_lib PUBLIC
    nlohmann_json::nlohmann_json
    cnpy
)

# Optional format support
if(HDF5_FOUND)
    target_compile_definitions(io_bench_lib PUBLIC HAVE_HDF5)
    target_link_libraries(io_bench_lib PUBLIC HighFive)
endif()

if(NetCDF_FOUND)
    target_compile_definitions(io_bench_lib PUBLIC HAVE_NETCDF)
    target_link_libraries(io_bench_lib PUBLIC NetCDF::NetCDF)
endif()

if(TileDB_FOUND)
    target_compile_definitions(io_bench_lib PUBLIC HAVE_TILEDB)
    target_link_libraries(io_bench_lib PUBLIC TileDB::tiledb_shared)
endif()

if(adios2_FOUND)
    target_compile_definitions(io_bench_lib PUBLIC HAVE_ADIOS2)
    target_link_libraries(io_bench_lib PUBLIC adios2::adios2)
endif()

if(DuckDB_FOUND)
    target_compile_definitions(io_bench_lib PUBLIC HAVE_DUCKDB)
endif()

# Benchmark executable
add_executable(io_bench src/main.cpp)
target_link_libraries(io_bench PRIVATE io_bench_lib benchmark::benchmark)

# Test executable
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(io_bench_tests
        tests/test_benchmark.cpp
        tests/test_formats.cpp
    )
    target_link_libraries(io_bench_tests PRIVATE io_bench_lib GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(io_bench_tests)
endif()
