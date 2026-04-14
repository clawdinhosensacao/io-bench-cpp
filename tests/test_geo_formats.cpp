#include <gtest/gtest.h>
#include "io_bench/formats.hpp"
#include <filesystem>
#include <vector>
#include <cmath>
#include <fstream>

class GeoFormatTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() / "io_bench_geo_test";
        std::filesystem::create_directories(temp_dir_);

        // 2D test data
        shape2d_ = {10, 8};
        data2d_.resize(shape2d_.total());
        for (std::size_t i = 0; i < shape2d_.total(); ++i) {
            data2d_[i] = static_cast<float>(i) * 1.5F;
        }
        read2d_.resize(shape2d_.total());

        // 3D test data
        shape3d_ = {10, 8, 3};
        data3d_.resize(shape3d_.total());
        for (std::size_t i = 0; i < shape3d_.total(); ++i) {
            data3d_[i] = static_cast<float>(i) * 0.7F;
        }
        read3d_.resize(shape3d_.total());
    }

    void TearDown() override {
        std::filesystem::remove_all(temp_dir_);
    }

    std::filesystem::path temp_dir_;
    io_bench::ArrayShape shape2d_{10, 8, 1};
    io_bench::ArrayShape shape3d_{10, 8, 3};
    std::vector<float> data2d_;
    std::vector<float> read2d_;
    std::vector<float> data3d_;
    std::vector<float> read3d_;
};

// --- RSF (Madagascar) ---

TEST_F(GeoFormatTest, RsfWriteRead2D) {
    io_bench::RsfFormat format;
    ASSERT_TRUE(format.is_available());

    auto path = temp_dir_ / "test.rsf";
    format.write(path.string(), data2d_.data(), shape2d_);
    format.read(path.string(), read2d_.data(), shape2d_);

    for (std::size_t i = 0; i < shape2d_.total(); ++i) {
        EXPECT_FLOAT_EQ(data2d_[i], read2d_[i]) << "mismatch at index " << i;
    }
}

TEST_F(GeoFormatTest, RsfWriteRead3D) {
    io_bench::RsfFormat format;

    auto path = temp_dir_ / "test3d.rsf";
    format.write(path.string(), data3d_.data(), shape3d_);
    format.read(path.string(), read3d_.data(), shape3d_);

    for (std::size_t i = 0; i < shape3d_.total(); ++i) {
        EXPECT_FLOAT_EQ(data3d_[i], read3d_[i]) << "mismatch at index " << i;
    }
}

TEST_F(GeoFormatTest, RsfDataFileSize) {
    io_bench::RsfFormat format;

    auto path = temp_dir_ / "test.rsf";
    format.write(path.string(), data2d_.data(), shape2d_);

    // RSF uses directory layout: path/data.bin
    auto data_path = std::filesystem::path(path.string()) / "data.bin";
    ASSERT_TRUE(std::filesystem::exists(data_path));
    EXPECT_EQ(shape2d_.bytes(), std::filesystem::file_size(data_path));
}

TEST_F(GeoFormatTest, RsfHeaderExists) {
    io_bench::RsfFormat format;

    auto path = temp_dir_ / "test.rsf";
    format.write(path.string(), data2d_.data(), shape2d_);

    // RSF uses directory layout: path/header.rsf
    auto header_path = std::filesystem::path(path.string()) / "header.rsf";
    ASSERT_TRUE(std::filesystem::exists(header_path));
    EXPECT_GT(std::filesystem::file_size(header_path), 0u);
}

TEST_F(GeoFormatTest, RsfHeaderContainsDimensions) {
    io_bench::RsfFormat format;

    auto path = temp_dir_ / "test.rsf";
    format.write(path.string(), data2d_.data(), shape2d_);

    auto header_path = std::filesystem::path(path.string()) / "header.rsf";
    std::ifstream hf(header_path);
    std::string content((std::istreambuf_iterator<char>(hf)),
                         std::istreambuf_iterator<char>());

    // Verify header contains the correct dimension parameters
    EXPECT_NE(content.find("n1=10"), std::string::npos) << "Header missing n1";
    EXPECT_NE(content.find("n2=8"), std::string::npos) << "Header missing n2";
    EXPECT_NE(content.find("esize=4"), std::string::npos) << "Header missing esize";
    EXPECT_NE(content.find("native_float"), std::string::npos) << "Header missing data_format";
}

// --- SEG-D ---

TEST_F(GeoFormatTest, SegdWriteRead2D) {
    io_bench::SegDFormat format;
    if (!format.is_available()) GTEST_SKIP() << "SEG-D not available";

    auto path = temp_dir_ / "test.segd";
    format.write(path.string(), data2d_.data(), shape2d_);
    format.read(path.string(), read2d_.data(), shape2d_);

    for (std::size_t i = 0; i < shape2d_.total(); ++i) {
        EXPECT_FLOAT_EQ(data2d_[i], read2d_[i]) << "mismatch at index " << i;
    }
}

TEST_F(GeoFormatTest, SegdAvailability) {
    io_bench::SegDFormat format;
    (void)format.is_available();
}

TEST_F(GeoFormatTest, SegdWriteRead3D) {
    io_bench::SegDFormat format;
    if (!format.is_available()) GTEST_SKIP() << "SEG-D not available";

    auto path = temp_dir_ / "test3d.segd";
    format.write(path.string(), data3d_.data(), shape3d_);
    format.read(path.string(), read3d_.data(), shape3d_);

    for (std::size_t i = 0; i < shape3d_.total(); ++i) {
        EXPECT_FLOAT_EQ(data3d_[i], read3d_[i]) << "mismatch at index " << i;
    }
}

TEST_F(GeoFormatTest, SegdFileSize) {
    io_bench::SegDFormat format;
    if (!format.is_available()) GTEST_SKIP() << "SEG-D not available";

    auto path = temp_dir_ / "test.segd";
    format.write(path.string(), data2d_.data(), shape2d_);

    // SEG-D file should exist and have non-zero size
    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 0u);
}

TEST_F(GeoFormatTest, SegdAndBinaryConsistency) {
    // SEG-D round-trip should produce same data as binary (within float precision)
    io_bench::SegDFormat segd;
    io_bench::BinaryFormat bin;
    if (!segd.is_available()) GTEST_SKIP() << "SEG-D not available";

    auto segd_path = temp_dir_ / "test.segd";
    auto bin_path = temp_dir_ / "test.bin";

    segd.write(segd_path.string(), data2d_.data(), shape2d_);
    bin.write(bin_path.string(), data2d_.data(), shape2d_);

    std::vector<float> segd_read(shape2d_.total());
    std::vector<float> bin_read(shape2d_.total());
    segd.read(segd_path.string(), segd_read.data(), shape2d_);
    bin.read(bin_path.string(), bin_read.data(), shape2d_);

    for (std::size_t i = 0; i < shape2d_.total(); ++i) {
        EXPECT_FLOAT_EQ(segd_read[i], bin_read[i]);
    }
}

// --- SEG-Y (bulk round-trip) ---

TEST_F(GeoFormatTest, SegyWriteRead2D) {
    io_bench::SegyFormat format;
    if (!format.is_available()) GTEST_SKIP() << "SEG-Y not available";

    auto path = temp_dir_ / "test.segy";
    format.write(path.string(), data2d_.data(), shape2d_);
    format.read(path.string(), read2d_.data(), shape2d_);

    for (std::size_t i = 0; i < shape2d_.total(); ++i) {
        EXPECT_FLOAT_EQ(data2d_[i], read2d_[i]) << "mismatch at index " << i;
    }
}

// --- MiniSEED ---

TEST_F(GeoFormatTest, MiniSeedAvailability) {
    io_bench::MiniSeedFormat format;
    (void)format.is_available();
}

// --- ASDF ---

TEST_F(GeoFormatTest, AsdfAvailability) {
    io_bench::AsdfFormat format;
    (void)format.is_available();
}

TEST_F(GeoFormatTest, AsdfWriteRead2D) {
    io_bench::AsdfFormat format;
    if (!format.is_available()) GTEST_SKIP() << "ASDF not available";

    auto path = temp_dir_ / "test.asdf";
    format.write(path.string(), data2d_.data(), shape2d_);
    format.read(path.string(), read2d_.data(), shape2d_);

    for (std::size_t i = 0; i < shape2d_.total(); ++i) {
        EXPECT_FLOAT_EQ(data2d_[i], read2d_[i]) << "mismatch at index " << i;
    }
}

// --- TensorStore C++ ---

TEST_F(GeoFormatTest, TensorStoreWriteRead2D) {
    io_bench::TensorStoreFormat format;
    if (!format.is_available()) GTEST_SKIP() << "TensorStore not available";

    auto path = temp_dir_ / "test.zarr";
    format.write(path.string(), data2d_.data(), shape2d_);
    format.read(path.string(), read2d_.data(), shape2d_);

    for (std::size_t i = 0; i < shape2d_.total(); ++i) {
        EXPECT_FLOAT_EQ(data2d_[i], read2d_[i]) << "mismatch at index " << i;
    }
}

// --- MDIO ---

TEST_F(GeoFormatTest, MdioAvailability) {
    io_bench::MdioFormat format;
    (void)format.is_available();
}

// --- Native geophysics format consistency ---

TEST_F(GeoFormatTest, RsfAndBinaryConsistency) {
    // RSF data file should be byte-identical to raw binary
    io_bench::RsfFormat rsf;
    io_bench::BinaryFormat bin;

    auto rsf_path = temp_dir_ / "test.rsf";
    auto bin_path = temp_dir_ / "test.bin";

    rsf.write(rsf_path.string(), data2d_.data(), shape2d_);
    bin.write(bin_path.string(), data2d_.data(), shape2d_);

    // Read both back
    std::vector<float> rsf_read(shape2d_.total());
    std::vector<float> bin_read(shape2d_.total());
    rsf.read(rsf_path.string(), rsf_read.data(), shape2d_);
    bin.read(bin_path.string(), bin_read.data(), shape2d_);

    for (std::size_t i = 0; i < shape2d_.total(); ++i) {
        EXPECT_FLOAT_EQ(rsf_read[i], bin_read[i]);
    }
}

// --- Slice Read Tests ---

TEST_F(GeoFormatTest, BinarySliceRead) {
    io_bench::BinaryFormat format;
    ASSERT_TRUE(format.supports_slice_read());

    auto path = temp_dir_ / "slice_test.bin";
    format.write(path.string(), data3d_.data(), shape3d_);

    // Read the middle inline (iy=1)
    const std::size_t iy = 1;
    const std::size_t slice_elements = shape3d_.nx * shape3d_.nz;
    std::vector<float> slice_buf(slice_elements);
    format.read_slice(path.string(), slice_buf.data(), shape3d_, iy);

    // Verify slice data matches the expected portion
    const float* expected = data3d_.data() + iy * slice_elements;
    for (std::size_t i = 0; i < slice_elements; ++i) {
        EXPECT_FLOAT_EQ(expected[i], slice_buf[i]) << "slice mismatch at " << i;
    }
}

TEST_F(GeoFormatTest, RsfSliceRead) {
    io_bench::RsfFormat format;
    ASSERT_TRUE(format.supports_slice_read());

    auto path = temp_dir_ / "slice_test.rsf";
    format.write(path.string(), data3d_.data(), shape3d_);

    const std::size_t iy = 1;
    const std::size_t slice_elements = shape3d_.nx * shape3d_.nz;
    std::vector<float> slice_buf(slice_elements);
    format.read_slice(path.string(), slice_buf.data(), shape3d_, iy);

    const float* expected = data3d_.data() + iy * slice_elements;
    for (std::size_t i = 0; i < slice_elements; ++i) {
        EXPECT_FLOAT_EQ(expected[i], slice_buf[i]) << "slice mismatch at " << i;
    }
}

TEST_F(GeoFormatTest, SliceReadVsFullRead) {
    // Slice read should produce same data as extracting from full read
    io_bench::BinaryFormat format;

    auto path = temp_dir_ / "slice_vs_full.bin";
    format.write(path.string(), data3d_.data(), shape3d_);

    // Full read
    std::vector<float> full_buf(shape3d_.total());
    format.read(path.string(), full_buf.data(), shape3d_);

    // Slice read
    const std::size_t iy = 2;
    const std::size_t slice_elements = shape3d_.nx * shape3d_.nz;
    std::vector<float> slice_buf(slice_elements);
    format.read_slice(path.string(), slice_buf.data(), shape3d_, iy);

    // Compare
    for (std::size_t i = 0; i < slice_elements; ++i) {
        EXPECT_FLOAT_EQ(full_buf[iy * slice_elements + i], slice_buf[i]);
    }
}

TEST_F(GeoFormatTest, Hdf5SliceRead) {
    io_bench::Hdf5Format format;
    if (!format.is_available()) GTEST_SKIP() << "HDF5 not available";
    ASSERT_TRUE(format.supports_slice_read());

    auto path = temp_dir_ / "slice_test.h5";
    format.write(path.string(), data3d_.data(), shape3d_);

    const std::size_t iy = 1;
    const std::size_t slice_elements = shape3d_.nx * shape3d_.nz;
    std::vector<float> slice_buf(slice_elements);
    format.read_slice(path.string(), slice_buf.data(), shape3d_, iy);

    const float* expected = data3d_.data() + iy * slice_elements;
    for (std::size_t i = 0; i < slice_elements; ++i) {
        EXPECT_FLOAT_EQ(expected[i], slice_buf[i]) << "slice mismatch at " << i;
    }
}

TEST_F(GeoFormatTest, TileDBSliceRead) {
    io_bench::TileDBFormat format;
    if (!format.is_available()) GTEST_SKIP() << "TileDB not available";
    ASSERT_TRUE(format.supports_slice_read());

    auto path = temp_dir_ / "slice_test.tiledb";
    format.write(path.string(), data3d_.data(), shape3d_);

    const std::size_t iy = 1;
    const std::size_t slice_elements = shape3d_.nx * shape3d_.nz;
    std::vector<float> slice_buf(slice_elements);
    format.read_slice(path.string(), slice_buf.data(), shape3d_, iy);

    const float* expected = data3d_.data() + iy * slice_elements;
    for (std::size_t i = 0; i < slice_elements; ++i) {
        EXPECT_FLOAT_EQ(expected[i], slice_buf[i]) << "slice mismatch at " << i;
    }
}

TEST_F(GeoFormatTest, NetcdfSliceRead) {
    io_bench::NetcdfFormat format;
    if (!format.is_available()) GTEST_SKIP() << "NetCDF not available";
    ASSERT_TRUE(format.supports_slice_read());

    auto path = temp_dir_ / "slice_test.nc";
    format.write(path.string(), data3d_.data(), shape3d_);

    // Full read to get reference data
    std::vector<float> full_buf(shape3d_.total());
    format.read(path.string(), full_buf.data(), shape3d_);

    const std::size_t iy = 1;
    const std::size_t slice_elements = shape3d_.nx * shape3d_.nz;
    std::vector<float> slice_buf(slice_elements);
    format.read_slice(path.string(), slice_buf.data(), shape3d_, iy);

    // NetCDF layout: (y, z, x) — iy-th inline is contiguous at offset iy * nz * nx
    for (std::size_t i = 0; i < slice_elements; ++i) {
        EXPECT_FLOAT_EQ(full_buf[iy * slice_elements + i], slice_buf[i])
            << "slice mismatch at " << i;
    }
}

TEST_F(GeoFormatTest, DuckDBSliceRead) {
    io_bench::DuckDBFormat format;
    if (!format.is_available()) GTEST_SKIP() << "DuckDB not available";
    ASSERT_TRUE(format.supports_slice_read());

    auto path = temp_dir_ / "slice_test.duckdb";
    format.write(path.string(), data3d_.data(), shape3d_);

    // Full read to get reference data
    std::vector<float> full_buf(shape3d_.total());
    format.read(path.string(), full_buf.data(), shape3d_);

    // Slice read
    const std::size_t iy = 1;
    const std::size_t slice_elements = shape3d_.nx * shape3d_.nz;
    std::vector<float> slice_buf(slice_elements);
    format.read_slice(path.string(), slice_buf.data(), shape3d_, iy);

    // Compare: slice should match the iy-th inline from full read
    // Full read layout is (iz, iy, ix) from DuckDB's ORDER BY
    // Slice layout is (iz, ix) — one inline
    for (std::size_t iz = 0; iz < shape3d_.nz; ++iz) {
        for (std::size_t ix = 0; ix < shape3d_.nx; ++ix) {
            std::size_t full_idx = iz * shape3d_.ny * shape3d_.nx + iy * shape3d_.nx + ix;
            std::size_t slice_idx = iz * shape3d_.nx + ix;
            EXPECT_FLOAT_EQ(full_buf[full_idx], slice_buf[slice_idx])
                << "mismatch at iz=" << iz << " ix=" << ix;
        }
    }
}

TEST_F(GeoFormatTest, ZarrSliceRead) {
    io_bench::ZarrFormat format;
    if (!format.is_available()) GTEST_SKIP() << "Zarr not available";
    ASSERT_TRUE(format.supports_slice_read());

    auto path = temp_dir_ / "slice_test.zarr";
    format.write(path.string(), data3d_.data(), shape3d_);

    // Full read to get reference data
    std::vector<float> full_buf(shape3d_.total());
    format.read(path.string(), full_buf.data(), shape3d_);

    // Slice read
    const std::size_t iy = 1;
    const std::size_t slice_elements = shape3d_.nx * shape3d_.nz;
    std::vector<float> slice_buf(slice_elements);
    format.read_slice(path.string(), slice_buf.data(), shape3d_, iy);

    // Compare: Zarr layout is (iz, iy, ix) in C-order
    for (std::size_t iz = 0; iz < shape3d_.nz; ++iz) {
        for (std::size_t ix = 0; ix < shape3d_.nx; ++ix) {
            std::size_t full_idx = iz * shape3d_.ny * shape3d_.nx + iy * shape3d_.nx + ix;
            std::size_t slice_idx = iz * shape3d_.nx + ix;
            EXPECT_FLOAT_EQ(full_buf[full_idx], slice_buf[slice_idx])
                << "mismatch at iz=" << iz << " ix=" << ix;
        }
    }
}

// --- Direct I/O Tests ---

TEST_F(GeoFormatTest, DirectIOAvailability) {
    io_bench::DirectIOFormat format;
    // On Linux should be available, on other platforms not
#ifdef __linux__
    EXPECT_TRUE(format.is_available());
#else
    EXPECT_FALSE(format.is_available());
#endif
}

TEST_F(GeoFormatTest, DirectIOWriteRead2D) {
    io_bench::DirectIOFormat format;
    if (!format.is_available()) GTEST_SKIP() << "Direct I/O not available on this platform";

    auto path = temp_dir_ / "test.dio";
    format.write(path.string(), data2d_.data(), shape2d_);
    format.read(path.string(), read2d_.data(), shape2d_);

    for (std::size_t i = 0; i < shape2d_.total(); ++i) {
        EXPECT_FLOAT_EQ(data2d_[i], read2d_[i]) << "mismatch at index " << i;
    }
}

TEST_F(GeoFormatTest, DirectIOCompressionRatio) {
    io_bench::DirectIOFormat format;
    if (!format.is_available()) GTEST_SKIP() << "Direct I/O not available";

    auto path = temp_dir_ / "test.dio";
    format.write(path.string(), data2d_.data(), shape2d_);

    // Direct I/O should have compression ratio ~1.0 (no compression)
    double file_size_mb = static_cast<double>(std::filesystem::file_size(path)) / (1024.0 * 1024.0);
    double raw_size_mb = shape2d_.mb();
    double ratio = raw_size_mb / file_size_mb;
    EXPECT_NEAR(ratio, 1.0, 0.1) << "Direct I/O should have ~1:1 compression ratio";
}

// --- Thread Safety Tests ---

TEST_F(GeoFormatTest, BinaryIsThreadSafe) {
    io_bench::BinaryFormat format;
    EXPECT_TRUE(format.is_thread_safe());
}

TEST_F(GeoFormatTest, RsfIsThreadSafe) {
    io_bench::RsfFormat format;
    EXPECT_TRUE(format.is_thread_safe());
}

TEST_F(GeoFormatTest, MdioNotThreadSafe) {
    io_bench::MdioFormat format;
    EXPECT_FALSE(format.is_thread_safe());
}

TEST_F(GeoFormatTest, TensorStoreThreadSafe) {
    io_bench::TensorStoreFormat format;
    // Native C++ API is thread-safe; Python bridge fallback is not
#ifdef HAVE_TENSORSTORE_CPP
    EXPECT_TRUE(format.is_thread_safe());
#else
    EXPECT_FALSE(format.is_thread_safe());
#endif
}

TEST_F(GeoFormatTest, AsdfNotThreadSafe) {
    io_bench::AsdfFormat format;
    EXPECT_FALSE(format.is_thread_safe());
}

TEST_F(GeoFormatTest, Hdf5NotThreadSafe) {
    io_bench::Hdf5Format format;
    EXPECT_FALSE(format.is_thread_safe());
}

// --- DuckDB ---

TEST_F(GeoFormatTest, DuckDBWriteRead2D) {
    io_bench::DuckDBFormat format;
    if (!format.is_available()) GTEST_SKIP() << "DuckDB not available";

    auto path = temp_dir_ / "test.duckdb";
    format.write(path.string(), data2d_.data(), shape2d_);
    format.read(path.string(), read2d_.data(), shape2d_);

    for (std::size_t i = 0; i < shape2d_.total(); ++i) {
        EXPECT_FLOAT_EQ(data2d_[i], read2d_[i]) << "mismatch at index " << i;
    }
}

TEST_F(GeoFormatTest, DuckDBWriteRead3D) {
    io_bench::DuckDBFormat format;
    if (!format.is_available()) GTEST_SKIP() << "DuckDB not available";

    auto path = temp_dir_ / "test3d.duckdb";
    format.write(path.string(), data3d_.data(), shape3d_);
    format.read(path.string(), read3d_.data(), shape3d_);

    for (std::size_t i = 0; i < shape3d_.total(); ++i) {
        EXPECT_FLOAT_EQ(data3d_[i], read3d_[i]) << "mismatch at index " << i;
    }
}

TEST_F(GeoFormatTest, DuckDBNotThreadSafe) {
    io_bench::DuckDBFormat format;
    EXPECT_FALSE(format.is_thread_safe());
}
