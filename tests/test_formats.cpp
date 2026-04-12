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
    (void)format.is_available();
}

TEST_F(FormatTest, NetcdfAvailability) {
    io_bench::NetcdfFormat format;
    (void)format.is_available();
}

TEST_F(FormatTest, TileDBAvailability) {
    io_bench::TileDBFormat format;
    (void)format.is_available();
}

TEST_F(FormatTest, Adios2Availability) {
    io_bench::Adios2Format format;
    (void)format.is_available();
}

TEST_F(FormatTest, DuckDBWriteRead) {
    io_bench::DuckDBFormat format;
    if (!format.is_available()) GTEST_SKIP() << "DuckDB not available";

    auto path = temp_dir_ / "test.duckdb";
    format.write(path.string(), data_.data(), shape_);
    format.read(path.string(), read_buffer_.data(), shape_);

    for (std::size_t i = 0; i < shape_.total(); ++i) {
        EXPECT_FLOAT_EQ(data_[i], read_buffer_[i]) << "mismatch at index " << i;
    }
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

TEST_F(FormatTest, SegyStreamingWriteAndRead) {
    io_bench::SegyFormat format;
    ASSERT_TRUE(format.is_available());
    ASSERT_TRUE(format.supports_stream_write());
    ASSERT_TRUE(format.supports_trace_read());

    auto path = temp_dir_ / "test_stream.segy";
    const std::size_t num_traces = shape_.nx * shape_.ny;
    const std::size_t samples_per_trace = shape_.nz;

    // Extract trace data from the flat array (SEG-Y trace layout)
    // Trace t at position (ix=t%nx, iy=t/nx) has z-samples at:
    //   data[iz * ny * nx + iy * nx + ix] for iz=0..nz-1
    auto extract_trace = [&](std::size_t t) -> std::vector<float> {
        std::vector<float> trace(samples_per_trace);
        const std::size_t ix = t % shape_.nx;
        const std::size_t iy = t / shape_.nx;
        for (std::size_t iz = 0; iz < samples_per_trace; ++iz) {
            std::size_t idx = (iz * shape_.ny * shape_.nx) + (iy * shape_.nx) + ix;
            trace[iz] = data_[idx];
        }
        return trace;
    };

    // Write traces one by one using streaming write
    for (std::size_t t = 0; t < num_traces; ++t) {
        auto trace = extract_trace(t);
        format.write_trace(path.string(), trace.data(), shape_, t);
    }

    // Read back via bulk read and verify
    std::vector<float> read_data(shape_.total());
    format.read(path.string(), read_data.data(), shape_);

    for (std::size_t i = 0; i < shape_.total(); ++i) {
        EXPECT_FLOAT_EQ(data_[i], read_data[i]) << "Mismatch at index " << i;
    }

    // Also test trace-by-trace read
    for (std::size_t t = 0; t < num_traces; ++t) {
        std::vector<float> trace_buf(samples_per_trace);
        format.read_trace(path.string(), trace_buf.data(), shape_, t);
        auto expected = extract_trace(t);
        for (std::size_t iz = 0; iz < samples_per_trace; ++iz) {
            EXPECT_FLOAT_EQ(expected[iz], trace_buf[iz]) << "Trace " << t << " sample " << iz;
        }
    }
}

TEST_F(FormatTest, SegDStreamingWriteAndRead) {
    io_bench::SegDFormat format;
    ASSERT_TRUE(format.is_available());
    ASSERT_TRUE(format.supports_stream_write());
    ASSERT_TRUE(format.supports_trace_read());

    auto path = temp_dir_ / "test_stream.sgd";
    // SEG-D traces are z-rows: each trace has nx samples
    const std::size_t num_traces = shape_.nz * shape_.ny;
    const std::size_t samples_per_trace = shape_.nx;

    // Write traces one by one (SEG-D layout: trace t = data[t * nx .. (t+1)*nx])
    for (std::size_t t = 0; t < num_traces; ++t) {
        const float* trace_data = data_.data() + (t * samples_per_trace);
        format.write_trace(path.string(), trace_data, shape_, t);
    }

    // Read back via bulk read and verify
    std::vector<float> read_data(shape_.total());
    format.read(path.string(), read_data.data(), shape_);

    for (std::size_t i = 0; i < shape_.total(); ++i) {
        EXPECT_FLOAT_EQ(data_[i], read_data[i]) << "Mismatch at index " << i;
    }
}
