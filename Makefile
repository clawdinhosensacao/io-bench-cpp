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
       src/formats/adios2.cpp \
       third_party/cnpy/cnpy.cpp

build/io_bench: build fetch-deps $(SRCS) src/main.cpp
	$(CXX) $(CXXFLAGS) $(SRCS) src/main.cpp -lz -o $@

build/io_bench_tests: build fetch-deps $(SRCS) $(GTEST_DIR)
	$(CXX) $(CXXFLAGS) -I $(GTEST_DIR)/googletest/include \
		$(SRCS) tests/test_benchmark.cpp tests/test_formats.cpp \
		$(GTEST_DIR)/googletest/src/gtest-all.cc \
		$(GTEST_DIR)/googletest/src/gtest_main.cc \
		-I $(GTEST_DIR)/googletest \
		-lz -lpthread -o $@

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

# Optional format support detection
HAVE_HDF5 := $(shell pkg-config --exists hdf5 && echo yes)
ifeq ($(HAVE_HDF5),yes)
CXXFLAGS += -DHAVE_HDF5 $(shell pkg-config --cflags hdf5)
LDFLAGS += $(shell pkg-config --libs hdf5)
endif

HAVE_NETCDF := $(shell pkg-config --exists netcdf && echo yes)
ifeq ($(HAVE_NETCDF),yes)
CXXFLAGS += -DHAVE_NETCDF $(shell pkg-config --cflags netcdf)
LDFLAGS += $(shell pkg-config --libs netcdf)
endif
