#include <gtest/gtest.h>
#include "io_bench/formats.hpp"
#include <filesystem>
#include <vector>

class FormatTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() / "io_bench_test";
        std::filesystem::create_directories(temp_dir_);
        
        // Generate test data
        shape_ = {10, 8};
        data_.resize(shape_.total());
        for (std::size_t i = 0; i < shape_.total(); ++i) {
            data_[i] = static_cast<float>(i) * 1.5f;
        }
        read_buffer_.resize(shape_.total());
    }
    
    void TearDown() override {
        std::filesystem::remove_all(temp_dir_);
    }
    
    std::filesystem::path temp_dir_;
    io_bench::ArrayShape shape_;
    std::vector<float> data_;
    std::vector<float> read_buffer_;
};

TEST_F(FormatTest, BinaryWriteRead) {
    io_bench::BinaryFormat format;
    
    auto path = temp_dir_ / "test.bin";
    format.write(path.string(), data_.data(), shape_);
    format.read(path.string(), read_buffer_.data(), shape_);
    
    for (std::size_t i = 0; i < shape_.total(); ++i) {
        EXPECT_FLOAT_EQ(data_[i], read_buffer_[i]);
    }
}

TEST_F(FormatTest, BinaryHeaderWriteRead) {
    io_bench::BinaryHeaderFormat format;
    
    auto path = temp_dir_ / "test.binh";
    format.write(path.string(), data_.data(), shape_);
    format.read(path.string(), read_buffer_.data(), shape_);
    
    for (std::size_t i = 0; i < shape_.total(); ++i) {
        EXPECT_FLOAT_EQ(data_[i], read_buffer_[i]);
    }
}

TEST_F(FormatTest, NpyWriteRead) {
    io_bench::NpyFormat format;
    ASSERT_TRUE(format.is_available());
    
    auto path = temp_dir_ / "test.npy";
    format.write(path.string(), data_.data(), shape_);
    format.read(path.string(), read_buffer_.data(), shape_);
    
    for (std::size_t i = 0; i < shape_.total(); ++i) {
        EXPECT_FLOAT_EQ(data_[i], read_buffer_[i]);
    }
}

TEST_F(FormatTest, JsonWriteRead) {
    io_bench::JsonFormat format;
    
    auto path = temp_dir_ / "test.json";
    format.write(path.string(), data_.data(), shape_);
    format.read(path.string(), read_buffer_.data(), shape_);
    
    for (std::size_t i = 0; i < shape_.total(); ++i) {
        EXPECT_FLOAT_EQ(data_[i], read_buffer_[i]);
    }
}

TEST_F(FormatTest, MmapWriteRead) {
    io_bench::MmapFormat format;
    
    auto path = temp_dir_ / "test.mmap";
    format.write(path.string(), data_.data(), shape_);
    format.read(path.string(), read_buffer_.data(), shape_);
    
    for (std::size_t i = 0; i < shape_.total(); ++i) {
        EXPECT_FLOAT_EQ(data_[i], read_buffer_[i]);
    }
}

TEST_F(FormatTest, Hdf5Availability) {
    io_bench::Hdf5Format format;
    // Just check that availability check doesn't throw
    EXPECT_NO_THROW(format.is_available());
}

TEST_F(FormatTest, NetcdfAvailability) {
    io_bench::NetcdfFormat format;
    EXPECT_NO_THROW(format.is_available());
}

TEST_F(FormatTest, TileDBAvailability) {
    io_bench::TileDBFormat format;
    EXPECT_NO_THROW(format.is_available());
}

TEST_F(FormatTest, Adios2Availability) {
    io_bench::Adios2Format format;
    EXPECT_NO_THROW(format.is_available());
}

TEST_F(FormatTest, BinaryFileSize) {
    io_bench::BinaryFormat format;
    
    auto path = temp_dir_ / "test.bin";
    format.write(path.string(), data_.data(), shape_);
    
    auto expected_size = shape_.bytes();
    auto actual_size = std::filesystem::file_size(path);
    
    EXPECT_EQ(expected_size, actual_size);
}

TEST_F(FormatTest, BinaryHeaderFileSize) {
    io_bench::BinaryHeaderFormat format;
    
    auto path = temp_dir_ / "test.binh";
    format.write(path.string(), data_.data(), shape_);
    
    // Header: 4 magic + 8 nx + 8 nz + 8 ny = 28 bytes
    auto expected_size = shape_.bytes() + 28;
    auto actual_size = std::filesystem::file_size(path);
    
    EXPECT_EQ(expected_size, actual_size);
}
