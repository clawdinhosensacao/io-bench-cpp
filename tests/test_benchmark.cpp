#include <gtest/gtest.h>
#include "io_bench/benchmark.hpp"
#include "io_bench/formats.hpp"
#include <thread>
#include <fstream>

class BenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.nx = 10;
        config_.nz = 8;
        config_.iterations = 1;
        config_.warmup = false;
    }
    
    io_bench::BenchConfig config_;
};

TEST_F(BenchmarkTest, GenerateDataReturnsCorrectSize) {
    io_bench::BenchmarkRunner runner(config_);
    io_bench::ArrayShape shape{config_.nx, config_.nz};
    
    auto data = runner.generate_data(shape);
    EXPECT_EQ(data.size(), shape.total());
}

TEST_F(BenchmarkTest, GenerateDataIsReproducible) {
    io_bench::BenchmarkRunner runner(config_);
    io_bench::ArrayShape shape{config_.nx, config_.nz};
    
    auto data1 = runner.generate_data(shape, 42);
    auto data2 = runner.generate_data(shape, 42);
    
    EXPECT_EQ(data1, data2);
}

TEST_F(BenchmarkTest, GenerateDataVariesWithSeed) {
    io_bench::BenchmarkRunner runner(config_);
    io_bench::ArrayShape shape{config_.nx, config_.nz};
    
    auto data1 = runner.generate_data(shape, 42);
    auto data2 = runner.generate_data(shape, 123);
    
    EXPECT_NE(data1, data2);
}

TEST_F(BenchmarkTest, ArrayShapeCalculatesCorrectly) {
    io_bench::ArrayShape shape{100, 80};
    
    EXPECT_EQ(shape.total(), 8000);
    EXPECT_EQ(shape.bytes(), 8000 * sizeof(float));
    EXPECT_NEAR(shape.mb(), 8000 * sizeof(float) / (1024.0 * 1024.0), 0.001);
}

TEST_F(BenchmarkTest, TimerMeasuresElapsed) {
    io_bench::Timer timer;
    timer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    timer.stop();
    
    EXPECT_GE(timer.elapsed_ms(), 10.0);
    EXPECT_LT(timer.elapsed_ms(), 100.0);  // generous upper bound
}

TEST_F(BenchmarkTest, ThroughputCalculation) {
    EXPECT_NEAR(io_bench::throughput_mbps(100.0, 1.0), 100.0, 0.01);
    EXPECT_NEAR(io_bench::throughput_mbps(50.0, 0.5), 100.0, 0.01);
    EXPECT_EQ(io_bench::throughput_mbps(100.0, 0.0), 0.0);
}

TEST_F(BenchmarkTest, CreateAllAdaptersReturnsNonEmpty) {
    auto adapters = io_bench::create_all_adapters();
    EXPECT_FALSE(adapters.empty());
    
    // Check all have names
    for (const auto& a : adapters) {
        EXPECT_FALSE(a->name().empty());
    }
}

TEST_F(BenchmarkTest, BinaryFormatIsAvailable) {
    io_bench::BinaryFormat format;
    EXPECT_TRUE(format.is_available());
}

TEST_F(BenchmarkTest, NpyFormatIsAvailable) {
    io_bench::NpyFormat format;
    EXPECT_TRUE(format.is_available());
}

TEST_F(BenchmarkTest, JsonFormatIsAvailable) {
    io_bench::JsonFormat format;
    EXPECT_TRUE(format.is_available());
}

TEST_F(BenchmarkTest, BigVolumeShapeIsApprox300MB) {
    // 3d-big-volume preset: 401 × 401 × 501
    io_bench::ArrayShape big_shape{401, 401, 501};
    EXPECT_EQ(big_shape.total(), 401UL * 401UL * 501UL);
    EXPECT_NEAR(big_shape.mb(), 307.3, 0.5);
    EXPECT_TRUE(big_shape.is_3d());
}

TEST_F(BenchmarkTest, TraceReadResultDefaults) {
    io_bench::TraceReadResult result;
    EXPECT_TRUE(result.name.empty());
    EXPECT_FALSE(result.available);
    EXPECT_EQ(result.num_traces, 0u);
    EXPECT_EQ(result.samples_per_trace, 0u);
    EXPECT_DOUBLE_EQ(result.trace_size_kb, 0.0);
    EXPECT_DOUBLE_EQ(result.sequential_read_ms, 0.0);
    EXPECT_DOUBLE_EQ(result.sequential_mbps, 0.0);
    EXPECT_DOUBLE_EQ(result.random_read_ms, 0.0);
    EXPECT_DOUBLE_EQ(result.random_mbps, 0.0);
    EXPECT_DOUBLE_EQ(result.random_traces_read, 0.0);
    EXPECT_TRUE(result.error.empty());
}

TEST_F(BenchmarkTest, FormatAdapterDefaultTraceReadFallback) {
    // Default FormatAdapter::read_trace should fall back to full read + extract
    io_bench::BinaryFormat format;
    EXPECT_FALSE(format.supports_trace_read());

    // Write a small dataset
    io_bench::ArrayShape shape{4, 3};  // 4 traces, 3 samples each
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f};
    std::string path = std::filesystem::temp_directory_path() / "trace_test.bin";
    format.write(path, data.data(), shape);

    // Read trace 2 (should be {7, 8, 9})
    std::vector<float> trace_buf(3, 0.0f);
    format.read_trace(path, trace_buf.data(), shape, 2);
    EXPECT_FLOAT_EQ(trace_buf[0], 7.0f);
    EXPECT_FLOAT_EQ(trace_buf[1], 8.0f);
    EXPECT_FLOAT_EQ(trace_buf[2], 9.0f);

    std::filesystem::remove(path);
}

TEST_F(BenchmarkTest, StreamWriteResultDefaults) {
    io_bench::StreamWriteResult result;
    EXPECT_TRUE(result.name.empty());
    EXPECT_FALSE(result.available);
    EXPECT_EQ(result.num_traces, 0u);
    EXPECT_EQ(result.samples_per_trace, 0u);
    EXPECT_DOUBLE_EQ(result.trace_size_kb, 0.0);
    EXPECT_DOUBLE_EQ(result.stream_write_ms, 0.0);
    EXPECT_DOUBLE_EQ(result.stream_write_mbps, 0.0);
    EXPECT_DOUBLE_EQ(result.bulk_write_ms, 0.0);
    EXPECT_DOUBLE_EQ(result.bulk_write_mbps, 0.0);
    EXPECT_DOUBLE_EQ(result.slowdown, 0.0);
    EXPECT_TRUE(result.error.empty());
}

TEST_F(BenchmarkTest, CheckpointResultDefaults) {
    io_bench::CheckpointResult result;
    EXPECT_TRUE(result.name.empty());
    EXPECT_FALSE(result.available);
    EXPECT_DOUBLE_EQ(result.file_size_mb, 0.0);
    EXPECT_DOUBLE_EQ(result.write_ms, 0.0);
    EXPECT_DOUBLE_EQ(result.read_ms, 0.0);
    EXPECT_DOUBLE_EQ(result.round_trip_ms, 0.0);
    EXPECT_DOUBLE_EQ(result.write_mbps, 0.0);
    EXPECT_DOUBLE_EQ(result.read_mbps, 0.0);
    EXPECT_FALSE(result.integrity_ok);
    EXPECT_DOUBLE_EQ(result.max_abs_error, 0.0);
    EXPECT_TRUE(result.error.empty());
}

TEST_F(BenchmarkTest, FileSizeMbRegularFile) {
    // Write a small binary file and check file_size_mb
    io_bench::BinaryFormat format;
    io_bench::ArrayShape shape{10, 8};
    auto data = io_bench::BenchmarkRunner::generate_data(shape);
    auto path = std::filesystem::temp_directory_path() / "fs_test_file.bin";
    format.write(path.string(), data.data(), shape);

    double size_mb = io_bench::file_size_mb(path);
    double expected_mb = static_cast<double>(shape.bytes()) / (1024.0 * 1024.0);
    EXPECT_NEAR(size_mb, expected_mb, 0.001);

    std::filesystem::remove(path);
}

TEST_F(BenchmarkTest, FileSizeMbNonExistentPath) {
    auto path = std::filesystem::temp_directory_path() / "nonexistent_file_12345.bin";
    EXPECT_DOUBLE_EQ(io_bench::file_size_mb(path), 0.0);
}

TEST_F(BenchmarkTest, FileSizeMbDirectory) {
    // Create a temporary directory with a known-size file
    auto dir = std::filesystem::temp_directory_path() / "fs_test_dir";
    std::filesystem::create_directories(dir);
    std::vector<float> data(100, 1.0f);
    auto file_path = dir / "data.bin";
    {
        std::ofstream f(file_path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(float));
    }

    double size_mb = io_bench::file_size_mb(dir);
    double expected_mb = static_cast<double>(100 * sizeof(float)) / (1024.0 * 1024.0);
    EXPECT_NEAR(size_mb, expected_mb, 0.001);

    std::filesystem::remove_all(dir);
}
