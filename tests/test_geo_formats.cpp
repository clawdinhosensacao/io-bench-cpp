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
