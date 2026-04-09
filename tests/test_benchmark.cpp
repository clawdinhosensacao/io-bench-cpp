#include <gtest/gtest.h>
#include "io_bench/benchmark.hpp"
#include "io_bench/formats.hpp"
#include <thread>

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
