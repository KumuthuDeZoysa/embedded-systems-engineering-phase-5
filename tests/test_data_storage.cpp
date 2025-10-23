/**
 * @file test_data_storage.cpp
 * @brief Comprehensive tests for data storage functionality
 * @author EcoWatt Test Team
 * @date 2025-09-06
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../cpp/include/data_storage.hpp"
#include "../cpp/include/types.hpp"
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>

using namespace ecoWatt;
using namespace testing;

class DataStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test samples
        createTestSamples();
        
        // Setup test database path
        test_db_path_ = "test_ecoWatt.db";
        
        // Remove existing test database
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
        
        // Setup storage configurations
        setupStorageConfigs();
    }

    void TearDown() override {
        // Cleanup test database
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
    }

private:
    void createTestSamples() {
        auto now = std::chrono::system_clock::now();
        
        // Create samples for voltage register (0)
        for (int i = 0; i < 10; ++i) {
            auto timestamp = now - std::chrono::seconds(i * 10);
            RegisterValue raw_value = 2300 + i; // 230.0V + variation
            double scaled_value = raw_value / 10.0;
            
            test_samples_.emplace_back(timestamp, 0, "Voltage", raw_value, scaled_value, "V");
        }
        
        // Create samples for current register (1)
        for (int i = 0; i < 10; ++i) {
            auto timestamp = now - std::chrono::seconds(i * 10);
            RegisterValue raw_value = 450 + i; // 4.50A + variation
            double scaled_value = raw_value / 100.0;
            
            test_samples_.emplace_back(timestamp, 1, "Current", raw_value, scaled_value, "A");
        }
        
        // Create samples for power register (9)
        for (int i = 0; i < 5; ++i) {
            auto timestamp = now - std::chrono::seconds(i * 20);
            RegisterValue raw_value = 10500 + i * 100; // 10.5kW + variation
            double scaled_value = raw_value / 1000.0;
            
            test_samples_.emplace_back(timestamp, 9, "Power", raw_value, scaled_value, "kW");
        }
    }
    
    void setupStorageConfigs() {
        memory_config_.memory_retention_samples = 100;
        memory_config_.enable_persistent_storage = false;
        
        sqlite_config_.memory_retention_samples = 1000;
        sqlite_config_.enable_persistent_storage = true;
        sqlite_config_.database_path = test_db_path_;
        sqlite_config_.data_retention_days = 30;
        
        hybrid_config_.memory_retention_samples = 50;
        hybrid_config_.enable_persistent_storage = true;
        hybrid_config_.database_path = test_db_path_;
        hybrid_config_.data_retention_days = 7;
    }

protected:
    std::vector<AcquisitionSample> test_samples_;
    std::string test_db_path_;
    StorageConfig memory_config_;
    StorageConfig sqlite_config_;
    StorageConfig hybrid_config_;
    
    // Helper methods
    bool samplesEqual(const AcquisitionSample& a, const AcquisitionSample& b) {
        return a.register_address == b.register_address &&
               a.raw_value == b.raw_value &&
               std::abs(a.scaled_value - b.scaled_value) < 0.001 &&
               a.unit == b.unit;
    }
    
    std::vector<AcquisitionSample> getSamplesByRegister(RegisterAddress addr) {
        std::vector<AcquisitionSample> result;
        for (const auto& sample : test_samples_) {
            if (sample.register_address == addr) {
                result.push_back(sample);
            }
        }
        return result;
    }
};

// ============================================================================
// MEMORY DATA STORAGE TESTS
// ============================================================================

TEST_F(DataStorageTest, MemoryStorage_StoreSingleSample_Success) {
    MemoryDataStorage storage(1000);
    
    AcquisitionSample sample = test_samples_[0];
    storage.storeSample(sample);
    
    auto latest = storage.getLatestSample(sample.register_address);
    ASSERT_NE(latest, nullptr) << "Should retrieve stored sample";
    EXPECT_TRUE(samplesEqual(*latest, sample)) << "Retrieved sample should match stored sample";
}

TEST_F(DataStorageTest, MemoryStorage_StoreMultipleSamples_Success) {
    MemoryDataStorage storage(1000);
    
    std::vector<AcquisitionSample> voltage_samples = getSamplesByRegister(0);
    storage.storeSamples(voltage_samples);
    
    auto retrieved_samples = storage.getSamples(0);
    EXPECT_EQ(retrieved_samples.size(), voltage_samples.size()) 
        << "Should retrieve all stored samples";
    
    // Samples should be in reverse chronological order (newest first)
    for (size_t i = 0; i < std::min(retrieved_samples.size(), voltage_samples.size()); ++i) {
        EXPECT_TRUE(samplesEqual(retrieved_samples[i], voltage_samples[voltage_samples.size() - 1 - i]))
            << "Sample " << i << " should match (in reverse order)";
    }
}

TEST_F(DataStorageTest, MemoryStorage_MaxSamplesLimit_EnforcesLimit) {
    MemoryDataStorage storage(5); // Small limit
    
    // Store more samples than the limit
    std::vector<AcquisitionSample> voltage_samples = getSamplesByRegister(0);
    ASSERT_GT(voltage_samples.size(), 5) << "Test should have more samples than limit";
    
    storage.storeSamples(voltage_samples);
    
    auto retrieved_samples = storage.getSamples(0);
    EXPECT_LE(retrieved_samples.size(), 5) << "Should not exceed maximum samples limit";
    
    // Should keep the newest samples
    if (!retrieved_samples.empty() && !voltage_samples.empty()) {
        EXPECT_TRUE(samplesEqual(retrieved_samples[0], voltage_samples.back()))
            << "Should keep the newest sample";
    }
}

TEST_F(DataStorageTest, MemoryStorage_GetSamplesByTimeRange_FiltersCorrectly) {
    MemoryDataStorage storage(1000);
    
    std::vector<AcquisitionSample> voltage_samples = getSamplesByRegister(0);
    storage.storeSamples(voltage_samples);
    
    // Define time range (middle 5 samples)
    auto start_time = voltage_samples[2].timestamp;
    auto end_time = voltage_samples[7].timestamp;
    
    auto filtered_samples = storage.getSamplesByTimeRange(0, start_time, end_time);
    
    EXPECT_GE(filtered_samples.size(), 5) << "Should retrieve samples in time range";
    
    // All retrieved samples should be within time range
    for (const auto& sample : filtered_samples) {
        EXPECT_GE(sample.timestamp, start_time) << "Sample should be after start time";
        EXPECT_LE(sample.timestamp, end_time) << "Sample should be before end time";
    }
}

TEST_F(DataStorageTest, MemoryStorage_GetLatestSample_ReturnsNewest) {
    MemoryDataStorage storage(1000);
    
    std::vector<AcquisitionSample> voltage_samples = getSamplesByRegister(0);
    
    // Store samples one by one to ensure timestamp ordering
    for (const auto& sample : voltage_samples) {
        storage.storeSample(sample);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto latest = storage.getLatestSample(0);
    ASSERT_NE(latest, nullptr) << "Should have latest sample";
    
    // Latest should be the last stored sample
    EXPECT_TRUE(samplesEqual(*latest, voltage_samples.back()))
        << "Latest sample should be the last stored";
}

TEST_F(DataStorageTest, MemoryStorage_GetAllLatestSamples_ReturnsAllRegisters) {
    MemoryDataStorage storage(1000);
    
    // Store samples for multiple registers
    storage.storeSamples(test_samples_);
    
    auto all_latest = storage.getAllLatestSamples();
    
    // Should have latest samples for registers 0, 1, and 9
    EXPECT_GE(all_latest.size(), 3) << "Should have samples for multiple registers";
    EXPECT_TRUE(all_latest.find(0) != all_latest.end()) << "Should have voltage sample";
    EXPECT_TRUE(all_latest.find(1) != all_latest.end()) << "Should have current sample";
    EXPECT_TRUE(all_latest.find(9) != all_latest.end()) << "Should have power sample";
}

TEST_F(DataStorageTest, MemoryStorage_ClearSamples_RemovesData) {
    MemoryDataStorage storage(1000);
    
    storage.storeSamples(test_samples_);
    
    // Clear samples for register 0
    storage.clearSamples(0);
    
    auto voltage_samples = storage.getSamples(0);
    EXPECT_TRUE(voltage_samples.empty()) << "Voltage samples should be cleared";
    
    auto current_samples = storage.getSamples(1);
    EXPECT_FALSE(current_samples.empty()) << "Current samples should remain";
    
    // Clear all samples
    storage.clearSamples(0, true);
    
    auto all_samples = storage.getAllLatestSamples();
    EXPECT_TRUE(all_samples.empty()) << "All samples should be cleared";
}

TEST_F(DataStorageTest, MemoryStorage_Statistics_AccurateTracking) {
    MemoryDataStorage storage(1000);
    
    storage.storeSamples(test_samples_);
    
    auto stats = storage.getStatistics();
    
    EXPECT_EQ(stats.total_samples, test_samples_.size()) 
        << "Should track total sample count";
    EXPECT_GE(stats.samples_by_register.size(), 3) 
        << "Should track samples by register";
    EXPECT_GT(stats.samples_by_register[0], 0) 
        << "Should have voltage samples";
    EXPECT_GT(stats.samples_by_register[1], 0) 
        << "Should have current samples";
}

// ============================================================================
// SQLITE DATA STORAGE TESTS
// ============================================================================

TEST_F(DataStorageTest, SQLiteStorage_StoreSingleSample_Success) {
    SQLiteDataStorage storage(test_db_path_);
    
    AcquisitionSample sample = test_samples_[0];
    storage.storeSample(sample);
    
    auto retrieved_samples = storage.getSamples(sample.register_address, 1);
    ASSERT_EQ(retrieved_samples.size(), 1) << "Should retrieve stored sample";
    EXPECT_TRUE(samplesEqual(retrieved_samples[0], sample)) 
        << "Retrieved sample should match stored sample";
}

TEST_F(DataStorageTest, SQLiteStorage_StoreMultipleSamples_BatchOperation) {
    SQLiteDataStorage storage(test_db_path_);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Test batch storage
    storage.storeSamples(test_samples_);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Batch operation should be reasonably fast
    EXPECT_LT(duration.count(), 1000) << "Batch storage should complete within 1 second";
    
    // Verify all samples are stored
    auto voltage_samples = storage.getSamples(0);
    auto current_samples = storage.getSamples(1);
    auto power_samples = storage.getSamples(9);
    
    EXPECT_GT(voltage_samples.size(), 0) << "Should have voltage samples";
    EXPECT_GT(current_samples.size(), 0) << "Should have current samples";
    EXPECT_GT(power_samples.size(), 0) << "Should have power samples";
}

TEST_F(DataStorageTest, SQLiteStorage_GetSamplesByTimeRange_FiltersCorrectly) {
    SQLiteDataStorage storage(test_db_path_);
    
    storage.storeSamples(test_samples_);
    
    // Define time range
    auto now = std::chrono::system_clock::now();
    auto start_time = now - std::chrono::seconds(60);
    auto end_time = now - std::chrono::seconds(10);
    
    auto filtered_samples = storage.getSamplesByTimeRange(0, start_time, end_time);
    
    // All retrieved samples should be within time range
    for (const auto& sample : filtered_samples) {
        EXPECT_GE(sample.timestamp, start_time) << "Sample should be after start time";
        EXPECT_LE(sample.timestamp, end_time) << "Sample should be before end time";
    }
}

TEST_F(DataStorageTest, SQLiteStorage_ExportToCSV_CreatesValidFile) {
    SQLiteDataStorage storage(test_db_path_);
    
    storage.storeSamples(test_samples_);
    
    std::string csv_file = "test_export.csv";
    storage.exportToCSV(csv_file);
    
    // Check if file was created
    EXPECT_TRUE(std::filesystem::exists(csv_file)) << "CSV file should be created";
    
    // Check file size (should have some content)
    auto file_size = std::filesystem::file_size(csv_file);
    EXPECT_GT(file_size, 100) << "CSV file should have reasonable content";
    
    // Cleanup
    if (std::filesystem::exists(csv_file)) {
        std::filesystem::remove(csv_file);
    }
}

TEST_F(DataStorageTest, SQLiteStorage_CleanupOldData_RemovesExpired) {
    SQLiteDataStorage storage(test_db_path_);
    
    // Create old samples
    std::vector<AcquisitionSample> old_samples;
    auto old_time = std::chrono::system_clock::now() - std::chrono::hours(24 * 40); // 40 days old
    
    for (int i = 0; i < 5; ++i) {
        old_samples.emplace_back(old_time, 0, "Voltage", 2300, 230.0, "V");
        old_time += std::chrono::seconds(10);
    }
    
    storage.storeSamples(old_samples);
    storage.storeSamples(test_samples_); // Recent samples
    
    auto stats_before = storage.getStatistics();
    uint64_t total_before = stats_before.total_samples;
    
    // Cleanup data older than 30 days
    storage.cleanupOldData(30);
    
    auto stats_after = storage.getStatistics();
    uint64_t total_after = stats_after.total_samples;
    
    EXPECT_LT(total_after, total_before) << "Should remove old samples";
    EXPECT_GE(total_after, test_samples_.size()) << "Should keep recent samples";
}

// ============================================================================
// HYBRID DATA STORAGE TESTS
// ============================================================================

TEST_F(DataStorageTest, HybridStorage_StoreSample_BothStorages) {
    HybridDataStorage storage(hybrid_config_);
    
    AcquisitionSample sample = test_samples_[0];
    storage.storeSample(sample);
    
    // Should be in memory storage (recent)
    auto recent_samples = storage.getRecentSamples(sample.register_address, 10);
    EXPECT_GE(recent_samples.size(), 1) << "Should have sample in memory storage";
    
    // Should also be in persistent storage
    auto now = std::chrono::system_clock::now();
    auto start_time = now - std::chrono::hours(1);
    auto end_time = now + std::chrono::hours(1);
    
    auto historical_samples = storage.getHistoricalSamples(sample.register_address, start_time, end_time);
    EXPECT_GE(historical_samples.size(), 1) << "Should have sample in persistent storage";
}

TEST_F(DataStorageTest, HybridStorage_GetLatestSample_FromMemory) {
    HybridDataStorage storage(hybrid_config_);
    
    storage.storeSamples(test_samples_);
    
    auto latest_voltage = storage.getLatestSample(0);
    ASSERT_NE(latest_voltage, nullptr) << "Should have latest voltage sample";
    
    auto latest_current = storage.getLatestSample(1);
    ASSERT_NE(latest_current, nullptr) << "Should have latest current sample";
    
    // Latest samples should be from memory (fastest access)
    EXPECT_EQ(latest_voltage->register_address, 0) << "Should be voltage register";
    EXPECT_EQ(latest_current->register_address, 1) << "Should be current register";
}

TEST_F(DataStorageTest, HybridStorage_GetAllLatestSamples_Complete) {
    HybridDataStorage storage(hybrid_config_);
    
    storage.storeSamples(test_samples_);
    
    auto all_latest = storage.getAllLatestSamples();
    
    EXPECT_GE(all_latest.size(), 3) << "Should have samples for multiple registers";
    EXPECT_TRUE(all_latest.find(0) != all_latest.end()) << "Should have voltage";
    EXPECT_TRUE(all_latest.find(1) != all_latest.end()) << "Should have current";
    EXPECT_TRUE(all_latest.find(9) != all_latest.end()) << "Should have power";
}

TEST_F(DataStorageTest, HybridStorage_MemoryOverflow_MovesToPersistent) {
    // Configure small memory limit to force overflow
    StorageConfig small_memory_config = hybrid_config_;
    small_memory_config.memory_retention_samples = 5; // Very small
    
    HybridDataStorage storage(small_memory_config);
    
    // Store more samples than memory limit
    std::vector<AcquisitionSample> many_samples;
    auto now = std::chrono::system_clock::now();
    
    for (int i = 0; i < 20; ++i) {
        auto timestamp = now - std::chrono::seconds(i);
        many_samples.emplace_back(timestamp, 0, "Voltage", 2300 + i, 230.0 + i/10.0, "V");
    }
    
    storage.storeSamples(many_samples);
    
    // Memory should have limited samples
    auto recent_samples = storage.getRecentSamples(0, 100);
    EXPECT_LE(recent_samples.size(), small_memory_config.memory_retention_samples + 5)
        << "Memory should respect size limits";
    
    // But historical query should find more samples
    auto start_time = now - std::chrono::hours(1);
    auto end_time = now + std::chrono::hours(1);
    auto historical_samples = storage.getHistoricalSamples(0, start_time, end_time);
    EXPECT_GE(historical_samples.size(), many_samples.size())
        << "Persistent storage should have all samples";
}

TEST_F(DataStorageTest, HybridStorage_ExportToJSON_CompleteData) {
    HybridDataStorage storage(hybrid_config_);
    
    storage.storeSamples(test_samples_);
    
    std::string json_file = "test_export.json";
    storage.exportToJSON(json_file);
    
    // Check if file was created
    EXPECT_TRUE(std::filesystem::exists(json_file)) << "JSON file should be created";
    
    // Check file size
    auto file_size = std::filesystem::file_size(json_file);
    EXPECT_GT(file_size, 100) << "JSON file should have reasonable content";
    
    // Cleanup
    if (std::filesystem::exists(json_file)) {
        std::filesystem::remove(json_file);
    }
}

TEST_F(DataStorageTest, HybridStorage_CombinedStatistics_Accurate) {
    HybridDataStorage storage(hybrid_config_);
    
    storage.storeSamples(test_samples_);
    
    auto combined_stats = storage.getCombinedStatistics();
    
    EXPECT_GT(combined_stats.memory_stats.total_samples, 0) 
        << "Memory should have samples";
    EXPECT_GT(combined_stats.persistent_stats.total_samples, 0) 
        << "Persistent storage should have samples";
    EXPECT_GT(combined_stats.total_storage_bytes, 0) 
        << "Should estimate storage usage";
    
    // Combined totals should be consistent
    EXPECT_EQ(combined_stats.memory_stats.total_samples + combined_stats.persistent_stats.total_samples,
              test_samples_.size() * 2) // Each sample stored in both memory and persistent
        << "Combined statistics should be consistent";
}

// ============================================================================
// THREAD SAFETY TESTS
// ============================================================================

TEST_F(DataStorageTest, MemoryStorage_ConcurrentAccess_ThreadSafe) {
    MemoryDataStorage storage(1000);
    
    const int num_threads = 5;
    const int samples_per_thread = 20;
    std::vector<std::thread> threads;
    std::atomic<int> stored_count{0};
    
    // Launch threads that store samples concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&storage, &stored_count, samples_per_thread, t]() {
            for (int i = 0; i < samples_per_thread; ++i) {
                RegisterAddress addr = t; // Each thread uses different register
                RegisterValue value = i + t * 1000;
                
                AcquisitionSample sample(std::chrono::system_clock::now(), 
                                       addr, "Test", value, value / 10.0, "V");
                
                storage.storeSample(sample);
                stored_count++;
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(stored_count.load(), num_threads * samples_per_thread)
        << "All samples should be stored safely";
    
    // Verify samples for each register
    for (int t = 0; t < num_threads; ++t) {
        auto samples = storage.getSamples(t);
        EXPECT_EQ(samples.size(), samples_per_thread)
            << "Register " << t << " should have all samples";
    }
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

TEST_F(DataStorageTest, MemoryStorage_Performance_FastAccess) {
    MemoryDataStorage storage(10000);
    
    // Store many samples
    std::vector<AcquisitionSample> many_samples;
    for (int i = 0; i < 1000; ++i) {
        many_samples.emplace_back(std::chrono::system_clock::now(), 0, "Voltage", 
                                 2300 + i, 230.0 + i/10.0, "V");
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    storage.storeSamples(many_samples);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto store_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    EXPECT_LT(store_duration.count(), 100) << "Memory storage should be fast";
    
    // Test retrieval speed
    start_time = std::chrono::high_resolution_clock::now();
    auto retrieved = storage.getSamples(0, 500);
    end_time = std::chrono::high_resolution_clock::now();
    
    auto retrieve_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    EXPECT_LT(retrieve_duration.count(), 50) << "Memory retrieval should be very fast";
    EXPECT_EQ(retrieved.size(), 500) << "Should retrieve requested number of samples";
}

TEST_F(DataStorageTest, SQLiteStorage_Performance_ReasonableSpeed) {
    SQLiteDataStorage storage(test_db_path_);
    
    // Test batch insertion performance
    std::vector<AcquisitionSample> batch_samples;
    for (int i = 0; i < 100; ++i) {
        batch_samples.emplace_back(std::chrono::system_clock::now(), 0, "Voltage", 
                                  2300 + i, 230.0 + i/10.0, "V");
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    storage.storeSamples(batch_samples);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    EXPECT_LT(duration.count(), 1000) << "SQLite batch storage should complete within 1 second";
    
    // Test query performance
    start_time = std::chrono::high_resolution_clock::now();
    auto retrieved = storage.getSamples(0, 50);
    end_time = std::chrono::high_resolution_clock::now();
    
    auto query_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    EXPECT_LT(query_duration.count(), 500) << "SQLite queries should be reasonably fast";
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "Running Data Storage Tests for EcoWatt Device\n";
    std::cout << "Testing memory, SQLite, and hybrid storage implementations\n";
    std::cout << "=========================================================\n\n";
    
    return RUN_ALL_TESTS();
}
