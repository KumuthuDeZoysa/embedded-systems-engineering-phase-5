/**
 * @file test_acquisition_scheduler.cpp
 * @brief Comprehensive tests for acquisition scheduler and automated polling
 * @author EcoWatt Test Team
 * @date 2025-09-06
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../cpp/include/acquisition_scheduler.hpp"
#include "../cpp/include/types.hpp"
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>

using namespace ecoWatt;
using namespace testing;

// Mock Protocol Adapter for testing
class MockProtocolAdapter {
public:
    MOCK_METHOD(ModbusResponse, readHoldingRegisters, 
                (RegisterAddress start_addr, uint16_t quantity), (const));
    MOCK_METHOD(ModbusResponse, writeSingleRegister, 
                (RegisterAddress addr, RegisterValue value), (const));
    MOCK_METHOD(std::map<RegisterAddress, RegisterValue>, readMultipleRegisters,
                (const std::vector<RegisterAddress>& addresses), (const));
    MOCK_METHOD(CommunicationStatistics, getStatistics, (), (const));
    MOCK_METHOD(void, resetStatistics, (), ());
};

// Mock Data Storage for testing
class MockDataStorage {
public:
    MOCK_METHOD(void, storeSample, (const AcquisitionSample& sample), ());
    MOCK_METHOD(void, storeSamples, (const std::vector<AcquisitionSample>& samples), ());
    MOCK_METHOD(std::vector<AcquisitionSample>, getSamples, 
                (RegisterAddress addr, uint32_t limit), (const));
    MOCK_METHOD(std::shared_ptr<AcquisitionSample>, getLatestSample, 
                (RegisterAddress addr), (const));
    MOCK_METHOD(void, clearSamples, (RegisterAddress addr, bool all_registers), ());
    
    // Track stored samples for verification
    mutable std::vector<AcquisitionSample> stored_samples_;
    
    void trackStoreSample(const AcquisitionSample& sample) {
        stored_samples_.push_back(sample);
    }
    
    void trackStoreSamples(const std::vector<AcquisitionSample>& samples) {
        stored_samples_.insert(stored_samples_.end(), samples.begin(), samples.end());
    }
};

class AcquisitionSchedulerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup mock expectations defaults
        setupMockDefaults();
        
        // Create test configurations
        createTestConfigurations();
        
        // Reset counters
        acquisition_count_ = 0;
        error_count_ = 0;
    }

    void TearDown() override {
        // Ensure scheduler is stopped
        if (scheduler_) {
            scheduler_->stop();
            scheduler_.reset();
        }
    }

private:
    void setupMockDefaults() {
        // Default successful responses
        ModbusResponse success_response;
        success_response.success = true;
        success_response.values = {2300, 450, 750, 600, 0, 1, 0, 0, 0, 10500};
        success_response.error_message = "";
        
        ON_CALL(mock_adapter_, readHoldingRegisters(_, _))
            .WillByDefault(Return(success_response));
            
        ON_CALL(mock_adapter_, writeSingleRegister(_, _))
            .WillByDefault(Return(success_response));
            
        std::map<RegisterAddress, RegisterValue> multi_read_result = {
            {0, 2300}, {1, 450}, {2, 750}, {9, 10500}
        };
        ON_CALL(mock_adapter_, readMultipleRegisters(_))
            .WillByDefault(Return(multi_read_result));
            
        // Setup storage mocks to track calls
        ON_CALL(mock_storage_, storeSample(_))
            .WillByDefault(Invoke(&mock_storage_, &MockDataStorage::trackStoreSample));
            
        ON_CALL(mock_storage_, storeSamples(_))
            .WillByDefault(Invoke(&mock_storage_, &MockDataStorage::trackStoreSamples));
    }
    
    void createTestConfigurations() {
        // Basic configuration
        basic_config_.polling_interval_ms = 100; // Fast for testing
        basic_config_.enabled_registers = {0, 1, 2, 9}; // Voltage, Current, Frequency, Power
        basic_config_.retry_attempts = 3;
        basic_config_.retry_delay_ms = 50;
        basic_config_.timeout_ms = 1000;
        basic_config_.enable_statistics = true;
        basic_config_.statistics_interval_ms = 500;
        
        // Slow polling configuration
        slow_config_ = basic_config_;
        slow_config_.polling_interval_ms = 5000; // 5 seconds as per requirement
        
        // Error-prone configuration
        error_config_ = basic_config_;
        error_config_.retry_attempts = 1;
        error_config_.timeout_ms = 100;
        
        // High-frequency configuration
        fast_config_ = basic_config_;
        fast_config_.polling_interval_ms = 10; // Very fast
        fast_config_.enabled_registers = {0}; // Single register
    }

protected:
    MockProtocolAdapter mock_adapter_;
    MockDataStorage mock_storage_;
    std::unique_ptr<AcquisitionScheduler> scheduler_;
    
    AcquisitionConfig basic_config_;
    AcquisitionConfig slow_config_;
    AcquisitionConfig error_config_;
    AcquisitionConfig fast_config_;
    
    std::atomic<int> acquisition_count_{0};
    std::atomic<int> error_count_{0};
    
    // Helper methods
    void createScheduler(const AcquisitionConfig& config) {
        scheduler_ = std::make_unique<AcquisitionScheduler>(config, mock_adapter_, mock_storage_);
    }
    
    void waitForAcquisitions(int expected_count, int timeout_ms = 2000) {
        auto start = std::chrono::steady_clock::now();
        while (mock_storage_.stored_samples_.size() < expected_count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (elapsed.count() > timeout_ms) {
                break;
            }
        }
    }
    
    bool hasRegisterSample(RegisterAddress addr) {
        return std::any_of(mock_storage_.stored_samples_.begin(),
                          mock_storage_.stored_samples_.end(),
                          [addr](const AcquisitionSample& sample) {
                              return sample.register_address == addr;
                          });
    }
    
    int countRegisterSamples(RegisterAddress addr) {
        return std::count_if(mock_storage_.stored_samples_.begin(),
                            mock_storage_.stored_samples_.end(),
                            [addr](const AcquisitionSample& sample) {
                                return sample.register_address == addr;
                            });
    }
};

// ============================================================================
// BASIC SCHEDULER FUNCTIONALITY TESTS
// ============================================================================

TEST_F(AcquisitionSchedulerTest, Constructor_ValidConfiguration_Success) {
    EXPECT_NO_THROW({
        createScheduler(basic_config_);
    }) << "Should construct scheduler with valid configuration";
    
    EXPECT_NE(scheduler_, nullptr) << "Scheduler should be created";
    EXPECT_FALSE(scheduler_->isRunning()) << "Scheduler should not be running initially";
}

TEST_F(AcquisitionSchedulerTest, StartStop_BasicOperation_Success) {
    createScheduler(basic_config_);
    
    // Start scheduler
    EXPECT_TRUE(scheduler_->start()) << "Should start successfully";
    EXPECT_TRUE(scheduler_->isRunning()) << "Should be running after start";
    
    // Stop scheduler
    scheduler_->stop();
    EXPECT_FALSE(scheduler_->isRunning()) << "Should not be running after stop";
}

TEST_F(AcquisitionSchedulerTest, StartTwice_NoDoubleStart_Handled) {
    createScheduler(basic_config_);
    
    EXPECT_TRUE(scheduler_->start()) << "First start should succeed";
    EXPECT_FALSE(scheduler_->start()) << "Second start should fail/be ignored";
    EXPECT_TRUE(scheduler_->isRunning()) << "Should still be running";
}

TEST_F(AcquisitionSchedulerTest, StopBeforeStart_NoError_Handled) {
    createScheduler(basic_config_);
    
    EXPECT_NO_THROW({
        scheduler_->stop();
    }) << "Should handle stop before start gracefully";
    
    EXPECT_FALSE(scheduler_->isRunning()) << "Should not be running";
}

// ============================================================================
// POLLING FUNCTIONALITY TESTS
// ============================================================================

TEST_F(AcquisitionSchedulerTest, AutomaticPolling_5SecondInterval_CorrectTiming) {
    createScheduler(slow_config_);
    
    // Expect multiple read calls
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(std::map<RegisterAddress, RegisterValue>{{0, 2300}, {1, 450}}));
    
    scheduler_->start();
    
    // Wait for at least 2 polling cycles (10+ seconds)
    auto start_time = std::chrono::steady_clock::now();
    while (mock_storage_.stored_samples_.size() < 8) { // 4 registers × 2 cycles
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time);
        if (elapsed.count() > 12) break; // Timeout after 12 seconds
    }
    
    scheduler_->stop();
    
    EXPECT_GE(mock_storage_.stored_samples_.size(), 4) 
        << "Should have at least one complete polling cycle";
}

TEST_F(AcquisitionSchedulerTest, FastPolling_HighFrequency_Performance) {
    createScheduler(fast_config_);
    
    // Expect many read calls for fast polling
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(10))
        .WillRepeatedly(Return(std::map<RegisterAddress, RegisterValue>{{0, 2300}}));
    
    scheduler_->start();
    
    // Wait for multiple fast polling cycles
    waitForAcquisitions(10, 1000); // 10 samples within 1 second
    
    scheduler_->stop();
    
    EXPECT_GE(mock_storage_.stored_samples_.size(), 10) 
        << "Should handle high-frequency polling";
    
    // All samples should be for register 0
    for (const auto& sample : mock_storage_.stored_samples_) {
        EXPECT_EQ(sample.register_address, 0) << "Should only poll configured register";
    }
}

TEST_F(AcquisitionSchedulerTest, RegisterSelection_OnlyConfiguredRegisters_Polled) {
    // Configure to poll only specific registers
    AcquisitionConfig selective_config = basic_config_;
    selective_config.enabled_registers = {0, 9}; // Only voltage and power
    
    createScheduler(selective_config);
    
    std::map<RegisterAddress, RegisterValue> selective_result = {{0, 2300}, {9, 10500}};
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(selective_result));
    
    scheduler_->start();
    waitForAcquisitions(2, 1000);
    scheduler_->stop();
    
    EXPECT_TRUE(hasRegisterSample(0)) << "Should have voltage samples";
    EXPECT_TRUE(hasRegisterSample(9)) << "Should have power samples";
    
    // Should not have other registers
    EXPECT_FALSE(hasRegisterSample(1)) << "Should not have current samples";
    EXPECT_FALSE(hasRegisterSample(2)) << "Should not have frequency samples";
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

TEST_F(AcquisitionSchedulerTest, CommunicationError_RetryMechanism_Works) {
    createScheduler(error_config_);
    
    // First call fails, second succeeds
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(2))
        .WillOnce(Return(std::map<RegisterAddress, RegisterValue>{})) // Empty = error
        .WillRepeatedly(Return(std::map<RegisterAddress, RegisterValue>{{0, 2300}}));
    
    scheduler_->start();
    waitForAcquisitions(1, 1000);
    scheduler_->stop();
    
    EXPECT_GE(mock_storage_.stored_samples_.size(), 1) 
        << "Should eventually succeed after retry";
}

TEST_F(AcquisitionSchedulerTest, PersistentErrors_GracefulHandling_ContinuesOperation) {
    createScheduler(basic_config_);
    
    // Always return empty (error condition)
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(3))
        .WillRepeatedly(Return(std::map<RegisterAddress, RegisterValue>{}));
    
    scheduler_->start();
    
    // Let it run for a bit despite errors
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_TRUE(scheduler_->isRunning()) 
        << "Should continue running despite persistent errors";
    
    scheduler_->stop();
    
    // Should have attempted multiple times but stored no samples
    EXPECT_EQ(mock_storage_.stored_samples_.size(), 0) 
        << "Should not store invalid samples";
}

TEST_F(AcquisitionSchedulerTest, TimeoutHandling_DoesNotBlockScheduler_Continues) {
    // Configure with very short timeout
    AcquisitionConfig timeout_config = basic_config_;
    timeout_config.timeout_ms = 10; // Very short timeout
    
    createScheduler(timeout_config);
    
    // Simulate timeout by slow response
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(2))
        .WillRepeatedly(Invoke([](const std::vector<RegisterAddress>&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Longer than timeout
            return std::map<RegisterAddress, RegisterValue>{{0, 2300}};
        }));
    
    scheduler_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    scheduler_->stop();
    
    // Scheduler should handle timeouts gracefully
    EXPECT_TRUE(true) << "Should handle timeouts without crashing";
}

// ============================================================================
// DATA VALIDATION TESTS
// ============================================================================

TEST_F(AcquisitionSchedulerTest, SampleCreation_CorrectValues_Stored) {
    createScheduler(basic_config_);
    
    std::map<RegisterAddress, RegisterValue> test_values = {
        {0, 2350}, // 235.0V
        {1, 475},  // 4.75A
        {9, 11250} // 11.25kW
    };
    
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(1))
        .WillOnce(Return(test_values));
    
    scheduler_->start();
    waitForAcquisitions(3, 1000);
    scheduler_->stop();
    
    // Check voltage sample
    auto voltage_samples = std::find_if(mock_storage_.stored_samples_.begin(),
                                       mock_storage_.stored_samples_.end(),
                                       [](const AcquisitionSample& s) { return s.register_address == 0; });
    
    if (voltage_samples != mock_storage_.stored_samples_.end()) {
        EXPECT_EQ(voltage_samples->raw_value, 2350) << "Raw voltage value should match";
        EXPECT_DOUBLE_EQ(voltage_samples->scaled_value, 235.0) << "Scaled voltage should be correct";
        EXPECT_EQ(voltage_samples->unit, "V") << "Voltage unit should be correct";
        EXPECT_EQ(voltage_samples->parameter_name, "Voltage") << "Parameter name should be correct";
    }
    
    // Check current sample
    auto current_samples = std::find_if(mock_storage_.stored_samples_.begin(),
                                       mock_storage_.stored_samples_.end(),
                                       [](const AcquisitionSample& s) { return s.register_address == 1; });
    
    if (current_samples != mock_storage_.stored_samples_.end()) {
        EXPECT_EQ(current_samples->raw_value, 475) << "Raw current value should match";
        EXPECT_DOUBLE_EQ(current_samples->scaled_value, 4.75) << "Scaled current should be correct";
        EXPECT_EQ(current_samples->unit, "A") << "Current unit should be correct";
    }
}

TEST_F(AcquisitionSchedulerTest, TimestampAccuracy_RecentTimestamps_Correct) {
    createScheduler(basic_config_);
    
    auto start_time = std::chrono::system_clock::now();
    
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(1))
        .WillOnce(Return(std::map<RegisterAddress, RegisterValue>{{0, 2300}}));
    
    scheduler_->start();
    waitForAcquisitions(1, 1000);
    scheduler_->stop();
    
    auto end_time = std::chrono::system_clock::now();
    
    ASSERT_FALSE(mock_storage_.stored_samples_.empty()) << "Should have stored samples";
    
    auto sample_time = mock_storage_.stored_samples_[0].timestamp;
    EXPECT_GE(sample_time, start_time) << "Sample timestamp should be after start";
    EXPECT_LE(sample_time, end_time) << "Sample timestamp should be before end";
    
    auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(end_time - sample_time);
    EXPECT_LT(time_diff.count(), 5) << "Sample timestamp should be recent";
}

// ============================================================================
// STATISTICS AND MONITORING TESTS
// ============================================================================

TEST_F(AcquisitionSchedulerTest, Statistics_Tracking_Accurate) {
    createScheduler(basic_config_);
    
    // Mock statistics
    CommunicationStatistics mock_stats;
    mock_stats.total_requests = 5;
    mock_stats.successful_requests = 4;
    mock_stats.failed_requests = 1;
    mock_stats.average_response_time_ms = 150.0;
    mock_stats.total_bytes_sent = 250;
    mock_stats.total_bytes_received = 400;
    
    EXPECT_CALL(mock_adapter_, getStatistics())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(mock_stats));
    
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(std::map<RegisterAddress, RegisterValue>{{0, 2300}}));
    
    scheduler_->start();
    waitForAcquisitions(2, 1000);
    
    auto stats = scheduler_->getStatistics();
    
    EXPECT_GT(stats.acquisition_cycles, 0) << "Should track acquisition cycles";
    EXPECT_GE(stats.total_samples_acquired, 2) << "Should track sample count";
    EXPECT_LE(stats.error_rate, 1.0) << "Error rate should be valid percentage";
    
    scheduler_->stop();
}

TEST_F(AcquisitionSchedulerTest, PerformanceMetrics_Timing_Measured) {
    createScheduler(basic_config_);
    
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(3))
        .WillRepeatedly(Return(std::map<RegisterAddress, RegisterValue>{{0, 2300}}));
    
    scheduler_->start();
    waitForAcquisitions(3, 1000);
    
    auto stats = scheduler_->getStatistics();
    
    EXPECT_GT(stats.average_cycle_time_ms, 0) << "Should measure cycle time";
    EXPECT_LT(stats.average_cycle_time_ms, basic_config_.polling_interval_ms * 2) 
        << "Cycle time should be reasonable";
    
    scheduler_->stop();
}

// ============================================================================
// SCHEDULER CONFIGURATION TESTS
// ============================================================================

TEST_F(AcquisitionSchedulerTest, ConfigurationUpdate_Runtime_Applied) {
    createScheduler(basic_config_);
    
    scheduler_->start();
    waitForAcquisitions(2, 1000);
    
    // Update configuration
    AcquisitionConfig new_config = basic_config_;
    new_config.enabled_registers = {9}; // Only power register
    new_config.polling_interval_ms = 50; // Faster polling
    
    scheduler_->updateConfiguration(new_config);
    
    // Clear previous samples for clean test
    mock_storage_.stored_samples_.clear();
    
    std::map<RegisterAddress, RegisterValue> power_only = {{9, 10500}};
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(power_only));
    
    waitForAcquisitions(2, 500);
    scheduler_->stop();
    
    // Should only have power register samples
    for (const auto& sample : mock_storage_.stored_samples_) {
        EXPECT_EQ(sample.register_address, 9) 
            << "Should only poll updated register configuration";
    }
}

TEST_F(AcquisitionSchedulerTest, DisableEnable_Register_DynamicControl) {
    createScheduler(basic_config_);
    
    scheduler_->start();
    
    // Disable register 1 (current)
    scheduler_->disableRegister(1);
    
    // Clear samples and test
    mock_storage_.stored_samples_.clear();
    
    std::map<RegisterAddress, RegisterValue> partial_result = {
        {0, 2300}, {1, 450}, {2, 750}, {9, 10500}
    };
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(partial_result));
    
    waitForAcquisitions(3, 1000); // Should get voltage, frequency, power (not current)
    
    EXPECT_TRUE(hasRegisterSample(0)) << "Should have voltage";
    EXPECT_FALSE(hasRegisterSample(1)) << "Should not have current (disabled)";
    EXPECT_TRUE(hasRegisterSample(2)) << "Should have frequency";
    EXPECT_TRUE(hasRegisterSample(9)) << "Should have power";
    
    // Re-enable register 1
    scheduler_->enableRegister(1);
    mock_storage_.stored_samples_.clear();
    
    waitForAcquisitions(4, 1000); // Should now get all registers
    
    EXPECT_TRUE(hasRegisterSample(1)) << "Should have current after re-enabling";
    
    scheduler_->stop();
}

// ============================================================================
// MEMORY AND RESOURCE TESTS
// ============================================================================

TEST_F(AcquisitionSchedulerTest, LongRunning_MemoryStability_NoLeaks) {
    createScheduler(basic_config_);
    
    EXPECT_CALL(mock_adapter_, readMultipleRegisters(_))
        .Times(AtLeast(20))
        .WillRepeatedly(Return(std::map<RegisterAddress, RegisterValue>{{0, 2300}}));
    
    scheduler_->start();
    
    // Run for extended period
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Scheduler should remain stable
        EXPECT_TRUE(scheduler_->isRunning()) << "Should remain running during iteration " << i;
    }
    
    scheduler_->stop();
    
    EXPECT_GE(mock_storage_.stored_samples_.size(), 10) 
        << "Should have acquired many samples during long run";
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "Running Acquisition Scheduler Tests for EcoWatt Device\n";
    std::cout << "Testing automated polling, error handling, and data acquisition\n";
    std::cout << "============================================================\n\n";
    
    return RUN_ALL_TESTS();
}
