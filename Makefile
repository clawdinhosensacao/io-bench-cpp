CXX ?= g++
CXXFLAGS = -O3 -std=c++20 -Wall -Wextra -Wpedantic -I include

# Third-party directories
JSON_DIR = third_party/json
CNPY_DIR = third_party/cnpy
GTEST_DIR = third_party/googletest

.PHONY: all clean test bench fetch-deps

all: build/io_bench

fetch-deps:
	@mkdir -p third_party
	@if [ ! -d "$(JSON_DIR)" ]; then \
		echo "Fetching nlohmann/json..."; \
		git clone --depth 1 https://github.com/nlohmann/json.git $(JSON_DIR); \
	fi
	@if [ ! -d "$(CNPY_DIR)" ]; then \
		echo "Fetching cnpy..."; \
		git clone --depth 1 https://github.com/rogersce/cnpy.git $(CNPY_DIR); \
	fi
	@if [ ! -d "$(GTEST_DIR)" ]; then \
		echo "Fetching googletest..."; \
		git clone --depth 1 --branch v1.14.0 https://github.com/google/googletest.git $(GTEST_DIR); \
	fi

build:
	mkdir -p build

# Include paths for third-party libraries
CXXFLAGS += -I third_party/json/include -I third_party/cnpy

# Sources
SRCS = src/benchmark.cpp src/report.cpp src/formats.cpp \
       src/formats/binary.cpp src/formats/npy.cpp src/formats/json.cpp \
       src/formats/hdf5.cpp src/formats/netcdf.cpp src/formats/tiledb.cpp \
       src/formats/adios2.cpp src/formats/extra.cpp \
       src/formats/zarr.cpp src/formats/segy.cpp src/formats/duckdb.cpp \
       src/formats/parquet.cpp src/formats/tensorstore.cpp \
       src/formats/mdio.cpp src/formats/miniseed.cpp \
       src/formats/rsf.cpp \
       third_party/cnpy/cnpy.cpp

build/io_bench: build fetch-deps $(SRCS) src/main.cpp
	$(CXX) $(CXXFLAGS) $(SRCS) src/main.cpp -lz $(LDFLAGS) -o $@

build/io_bench_tests: build fetch-deps $(SRCS) $(GTEST_DIR)
	$(CXX) $(CXXFLAGS) -I $(GTEST_DIR)/googletest/include \
		$(SRCS) tests/test_benchmark.cpp tests/test_formats.cpp \
		$(GTEST_DIR)/googletest/src/gtest-all.cc \
		$(GTEST_DIR)/googletest/src/gtest_main.cc \
		-I $(GTEST_DIR)/googletest \
		-lz $(LDFLAGS) -lpthread -o $@

test: build/io_bench_tests
	./build/io_bench_tests

bench: build/io_bench
	mkdir -p artifacts
	./build/io_bench --nx 80 --nz 100 --iterations 3 --output artifacts/benchmark.md

bench-fast: build/io_bench
	mkdir -p artifacts
	./build/io_bench --nx 40 --nz 50 --iterations 1 --output artifacts/benchmark_fast.md

clean:
	rm -rf build artifacts

# Optional format support detection (linuxbrew paths)
HDF5_PREFIX := /home/linuxbrew/.linuxbrew
HDF5_EXISTS := $(shell test -f $(HDF5_PREFIX)/include/hdf5.h && echo yes)
ifeq ($(HDF5_EXISTS),yes)
CXXFLAGS += -DHAVE_HDF5 -I$(HDF5_PREFIX)/include
LDFLAGS += -L$(HDF5_PREFIX)/lib -lhdf5 -lhdf5_hl -Wl,-rpath,$(HDF5_PREFIX)/lib -Wl,-rpath-link,$(HDF5_PREFIX)/lib
endif

NETCDF_EXISTS := $(shell test -f $(HDF5_PREFIX)/include/netcdf.h && echo yes)
ifeq ($(NETCDF_EXISTS),yes)
CXXFLAGS += -DHAVE_NETCDF -I$(HDF5_PREFIX)/include
LDFLAGS += -L$(HDF5_PREFIX)/lib -lnetcdf -Wl,-rpath,$(HDF5_PREFIX)/lib -Wl,-rpath-link,$(HDF5_PREFIX)/lib
endif

TILEDB_EXISTS := $(shell test -f $(HDF5_PREFIX)/include/tiledb/tiledb && echo yes)
ifeq ($(TILEDB_EXISTS),yes)
CXXFLAGS += -DHAVE_TILEDB -I$(HDF5_PREFIX)/include
LDFLAGS += -L$(HDF5_PREFIX)/lib -ltiledb -Wl,-rpath,$(HDF5_PREFIX)/lib -Wl,-rpath-link,$(HDF5_PREFIX)/lib
endif

ADIOS2_EXISTS := $(shell test -f $(HDF5_PREFIX)/include/adios2_c.h && echo yes)
ifeq ($(ADIOS2_EXISTS),yes)
CXXFLAGS += -DHAVE_ADIOS2 -I$(HDF5_PREFIX)/include
LDFLAGS += -L$(HDF5_PREFIX)/lib -ladios2 -Wl,-rpath,$(HDF5_PREFIX)/lib -Wl,-rpath-link,$(HDF5_PREFIX)/lib
endif

# DuckDB support
DUCKDB_EXISTS := $(shell test -f $(HDF5_PREFIX)/include/duckdb.h && echo yes)
ifeq ($(DUCKDB_EXISTS),yes)
CXXFLAGS += -DHAVE_DUCKDB -I$(HDF5_PREFIX)/include
LDFLAGS += -L$(HDF5_PREFIX)/lib -lduckdb -Wl,-rpath,$(HDF5_PREFIX)/lib -Wl,-rpath-link,$(HDF5_PREFIX)/lib
endif

# Apache Arrow/Parquet support
PARQUET_EXISTS := $(shell test -f $(HDF5_PREFIX)/include/arrow/api.h && echo yes)
ifeq ($(PARQUET_EXISTS),yes)
CXXFLAGS += -DHAVE_PARQUET -I$(HDF5_PREFIX)/include
LDFLAGS += -L$(HDF5_PREFIX)/lib -larrow -lparquet -Wl,-rpath,$(HDF5_PREFIX)/lib -Wl,-rpath-link,$(HDF5_PREFIX)/lib
endif
