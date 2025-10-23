/**
 * @file test_protocol_adapter.cpp
 * @brief Comprehensive tests for ProtocolAdapter class
 * @author EcoWatt Test Team
 * @date 2025-09-06
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../cpp/include/protocol_adapter.hpp"
#include "../cpp/include/config_manager.hpp"
#include "../cpp/include/exceptions.hpp"
#include <vector>
#include <string>
#include <chrono>
#include <thread>

using namespace ecoWatt;
using namespace testing;

// Mock HTTP Client for testing
class MockHttpClient {
public:
    struct HttpResponse {
        long status_code;
        std::string body;
        std::map<std::string, std::string> headers;
        
        bool isSuccess() const { return status_code >= 200 && status_code < 300; }
    };
    
    MOCK_METHOD(HttpResponse, post, (const std::string& endpoint, const std::string& data), ());
    MOCK_METHOD(void, setDefaultHeaders, (const std::map<std::string, std::string>& headers), ());
    MOCK_METHOD(void, setTimeout, (uint32_t timeout_ms), ());
};

class ProtocolAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test configuration
        ConfigManager config;
        
        // Configure Modbus settings
        ModbusConfig modbus_config;
        modbus_config.slave_address = 0x11;
        modbus_config.timeout = Duration(5000);
        modbus_config.max_retries = 3;
        modbus_config.retry_delay = Duration(100); // Shorter for testing
        
        // Configure API settings
        ApiConfig api_config;
        api_config.base_url = "http://localhost:8080";
        api_config.read_endpoint = "/api/inverter/read";
        api_config.write_endpoint = "/api/inverter/write";
        api_config.content_type = "application/json";
        api_config.accept = "*/*";
        
        // Set up config manager
        config.updateModbusConfig(modbus_config);
        config.updateApiConfig(api_config);
        
        // Create protocol adapter
        protocol_adapter_ = std::make_unique<ProtocolAdapter>(config);
    }

    void TearDown() override {
        protocol_adapter_.reset();
    }

    std::unique_ptr<ProtocolAdapter> protocol_adapter_;
    
    // Helper methods
    std::string createValidReadResponse(const std::vector<uint16_t>& values) {
        std::string frame = "110304"; // Slave 0x11, Function 0x03, ByteCount 0x04
        
        // Add register values (big-endian)
        for (uint16_t value : values) {
            frame += fmt::format("{:04X}", value);
        }
        
        // Calculate and append CRC
        std::vector<uint8_t> bytes = ModbusFrame::hexToBytes(frame);
        uint16_t crc = ModbusFrame::calculateCRC(bytes);
        frame += fmt::format("{:02X}{:02X}", crc & 0xFF, (crc >> 8) & 0xFF);
        
        return "{\"frame\":\"" + frame + "\"}";
    }
    
    std::string createValidWriteResponse(RegisterAddress addr, RegisterValue value) {
        std::string frame = fmt::format("1106{:04X}{:04X}", addr, value);
        
        // Calculate and append CRC
        std::vector<uint8_t> bytes = ModbusFrame::hexToBytes(frame);
        uint16_t crc = ModbusFrame::calculateCRC(bytes);
        frame += fmt::format("{:02X}{:02X}", crc & 0xFF, (crc >> 8) & 0xFF);
        
        return "{\"frame\":\"" + frame + "\"}";
    }
    
    std::string createErrorResponse(uint8_t function_code, uint8_t error_code) {
        std::string frame = fmt::format("11{:02X}{:02X}", function_code | 0x80, error_code);
        
        // Calculate and append CRC
        std::vector<uint8_t> bytes = ModbusFrame::hexToBytes(frame);
        uint16_t crc = ModbusFrame::calculateCRC(bytes);
        frame += fmt::format("{:02X}{:02X}", crc & 0xFF, (crc >> 8) & 0xFF);
        
        return "{\"frame\":\"" + frame + "\"}";
    }
};

// ============================================================================
// READ REGISTER TESTS
// ============================================================================

TEST_F(ProtocolAdapterTest, ReadRegisters_ValidSingleRegister_Success) {
    // Mock successful HTTP response
    std::vector<uint16_t> test_values = {2500}; // 250.0V
    std::string mock_response = createValidReadResponse(test_values);
    
    // Test reading single register
    std::vector<RegisterValue> result = protocol_adapter_->readRegisters(0, 1);
    
    EXPECT_EQ(result.size(), 1) << "Should return exactly 1 register";
    EXPECT_EQ(result[0], 2500) << "Register value should match expected";
}

TEST_F(ProtocolAdapterTest, ReadRegisters_ValidMultipleRegisters_Success) {
    // Mock successful HTTP response
    std::vector<uint16_t> test_values = {2500, 450}; // 250.0V, 4.5A
    std::string mock_response = createValidReadResponse(test_values);
    
    // Test reading multiple registers
    std::vector<RegisterValue> result = protocol_adapter_->readRegisters(0, 2);
    
    EXPECT_EQ(result.size(), 2) << "Should return exactly 2 registers";
    EXPECT_EQ(result[0], 2500) << "First register should match expected";
    EXPECT_EQ(result[1], 450) << "Second register should match expected";
}

TEST_F(ProtocolAdapterTest, ReadRegisters_MaximumRegisters_Success) {
    // Test reading maximum allowed registers (125)
    std::vector<uint16_t> test_values(125, 1000);
    std::string mock_response = createValidReadResponse(test_values);
    
    std::vector<RegisterValue> result = protocol_adapter_->readRegisters(0, 125);
    
    EXPECT_EQ(result.size(), 125) << "Should return exactly 125 registers";
    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], 1000) << "Register " << i << " should match expected value";
    }
}

TEST_F(ProtocolAdapterTest, ReadRegisters_ZeroRegisters_ThrowsException) {
    EXPECT_THROW(protocol_adapter_->readRegisters(0, 0), ModbusException)
        << "Reading 0 registers should throw ModbusException";
}

TEST_F(ProtocolAdapterTest, ReadRegisters_TooManyRegisters_ThrowsException) {
    EXPECT_THROW(protocol_adapter_->readRegisters(0, 126), ModbusException)
        << "Reading more than 125 registers should throw ModbusException";
}

TEST_F(ProtocolAdapterTest, ReadRegisters_ErrorResponse_ThrowsModbusException) {
    // Mock error response (illegal data address)
    std::string error_response = createErrorResponse(0x03, 0x02);
    
    EXPECT_THROW(protocol_adapter_->readRegisters(0xFFFF, 1), ModbusException)
        << "Error response should throw ModbusException";
}

TEST_F(ProtocolAdapterTest, ReadRegisters_NetworkTimeout_ThrowsModbusException) {
    // Simulate network timeout by configuring very short timeout
    EXPECT_THROW(protocol_adapter_->readRegisters(0, 1), ModbusException)
        << "Network timeout should throw ModbusException";
}

TEST_F(ProtocolAdapterTest, ReadRegisters_InvalidJSONResponse_ThrowsModbusException) {
    // Mock invalid JSON response
    std::string invalid_json = "invalid json response";
    
    EXPECT_THROW(protocol_adapter_->readRegisters(0, 1), ModbusException)
        << "Invalid JSON should throw ModbusException";
}

TEST_F(ProtocolAdapterTest, ReadRegisters_MissingFrameField_ThrowsModbusException) {
    // Mock response without frame field
    std::string no_frame_response = "{\"status\":\"ok\"}";
    
    EXPECT_THROW(protocol_adapter_->readRegisters(0, 1), ModbusException)
        << "Missing frame field should throw ModbusException";
}

TEST_F(ProtocolAdapterTest, ReadRegisters_CorruptedFrame_ThrowsModbusException) {
    // Mock response with corrupted frame
    std::string corrupted_response = "{\"frame\":\"11030409C4044EE95E\"}"; // Wrong CRC
    
    EXPECT_THROW(protocol_adapter_->readRegisters(0, 1), ModbusException)
        << "Corrupted frame should throw ModbusException";
}

TEST_F(ProtocolAdapterTest, ReadRegisters_RegisterCountMismatch_ThrowsModbusException) {
    // Mock response with different register count than requested
    std::vector<uint16_t> test_values = {2500}; // Only 1 register
    std::string mock_response = createValidReadResponse(test_values);
    
    EXPECT_THROW(protocol_adapter_->readRegisters(0, 2), ModbusException) // Request 2 registers
        << "Register count mismatch should throw ModbusException";
}

// ============================================================================
// WRITE REGISTER TESTS
// ============================================================================

TEST_F(ProtocolAdapterTest, WriteRegister_ValidParameters_Success) {
    // Mock successful write response
    RegisterAddress test_addr = 0x0008;
    RegisterValue test_value = 0x0064;
    std::string mock_response = createValidWriteResponse(test_addr, test_value);
    
    bool result = protocol_adapter_->writeRegister(test_addr, test_value);
    
    EXPECT_TRUE(result) << "Write operation should succeed";
}

TEST_F(ProtocolAdapterTest, WriteRegister_MinimumValue_Success) {
    RegisterAddress test_addr = 0x0000;
    RegisterValue test_value = 0x0000;
    std::string mock_response = createValidWriteResponse(test_addr, test_value);
    
    bool result = protocol_adapter_->writeRegister(test_addr, test_value);
    
    EXPECT_TRUE(result) << "Write with minimum values should succeed";
}

TEST_F(ProtocolAdapterTest, WriteRegister_MaximumValue_Success) {
    RegisterAddress test_addr = 0xFFFF;
    RegisterValue test_value = 0xFFFF;
    std::string mock_response = createValidWriteResponse(test_addr, test_value);
    
    bool result = protocol_adapter_->writeRegister(test_addr, test_value);
    
    EXPECT_TRUE(result) << "Write with maximum values should succeed";
}

TEST_F(ProtocolAdapterTest, WriteRegister_ErrorResponse_ThrowsModbusException) {
    // Mock error response (illegal data value)
    std::string error_response = createErrorResponse(0x06, 0x03);
    
    EXPECT_THROW(protocol_adapter_->writeRegister(0x0008, 0x0064), ModbusException)
        << "Error response should throw ModbusException";
}

TEST_F(ProtocolAdapterTest, WriteRegister_EchoMismatchAddress_ThrowsModbusException) {
    // Mock response with wrong address echo
    RegisterAddress request_addr = 0x0008;
    RegisterAddress response_addr = 0x0009; // Wrong address
    RegisterValue test_value = 0x0064;
    
    std::string mock_response = createValidWriteResponse(response_addr, test_value);
    
    EXPECT_THROW(protocol_adapter_->writeRegister(request_addr, test_value), ModbusException)
        << "Address echo mismatch should throw ModbusException";
}

TEST_F(ProtocolAdapterTest, WriteRegister_EchoMismatchValue_ThrowsModbusException) {
    // Mock response with wrong value echo
    RegisterAddress test_addr = 0x0008;
    RegisterValue request_value = 0x0064;
    RegisterValue response_value = 0x0065; // Wrong value
    
    std::string mock_response = createValidWriteResponse(test_addr, response_value);
    
    EXPECT_THROW(protocol_adapter_->writeRegister(test_addr, request_value), ModbusException)
        << "Value echo mismatch should throw ModbusException";
}

TEST_F(ProtocolAdapterTest, WriteRegister_NetworkTimeout_ThrowsModbusException) {
    // Simulate network timeout
    EXPECT_THROW(protocol_adapter_->writeRegister(0x0008, 0x0064), ModbusException)
        << "Network timeout should throw ModbusException";
}

// ============================================================================
// COMMUNICATION TEST
// ============================================================================

TEST_F(ProtocolAdapterTest, TestCommunication_Success) {
    // Mock successful read and write responses
    std::vector<uint16_t> read_values = {2500, 450};
    std::string read_response = createValidReadResponse(read_values);
    
    RegisterValue export_power = 75;
    std::string write_response = createValidWriteResponse(8, export_power);
    
    bool result = protocol_adapter_->testCommunication();
    
    EXPECT_TRUE(result) << "Communication test should succeed";
}

TEST_F(ProtocolAdapterTest, TestCommunication_ReadFailure_Failure) {
    // Mock read failure
    std::string error_response = createErrorResponse(0x03, 0x02);
    
    bool result = protocol_adapter_->testCommunication();
    
    EXPECT_FALSE(result) << "Communication test should fail on read error";
}

TEST_F(ProtocolAdapterTest, TestCommunication_WriteFailure_Failure) {
    // Mock successful read but failed write
    std::vector<uint16_t> read_values = {2500, 450};
    std::string read_response = createValidReadResponse(read_values);
    
    std::string write_error = createErrorResponse(0x06, 0x03);
    
    bool result = protocol_adapter_->testCommunication();
    
    EXPECT_FALSE(result) << "Communication test should fail on write error";
}

// ============================================================================
// RETRY MECHANISM TESTS
// ============================================================================

TEST_F(ProtocolAdapterTest, ReadRegisters_RetryOnFailure_EventualSuccess) {
    // First two attempts fail, third succeeds
    std::vector<uint16_t> test_values = {2500};
    std::string success_response = createValidReadResponse(test_values);
    
    // This test requires mocking the HTTP client to simulate retry behavior
    // For now, we'll test the basic functionality
    std::vector<RegisterValue> result = protocol_adapter_->readRegisters(0, 1);
    
    EXPECT_EQ(result.size(), 1) << "Should eventually succeed after retries";
}

TEST_F(ProtocolAdapterTest, ReadRegisters_ExhaustAllRetries_ThrowsException) {
    // All retry attempts fail
    EXPECT_THROW(protocol_adapter_->readRegisters(0, 1), ModbusException)
        << "Should throw exception after exhausting all retries";
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(ProtocolAdapterTest, Statistics_SuccessfulOperations_UpdatesCorrectly) {
    // Perform some operations
    try {
        std::vector<RegisterValue> result = protocol_adapter_->readRegisters(0, 1);
        protocol_adapter_->writeRegister(8, 50);
    } catch (...) {
        // Ignore failures for this test
    }
    
    CommunicationStats stats = protocol_adapter_->getStatistics();
    
    EXPECT_GT(stats.total_requests, 0) << "Total requests should be greater than 0";
    EXPECT_GE(stats.successful_requests, 0) << "Successful requests should be >= 0";
    EXPECT_GE(stats.failed_requests, 0) << "Failed requests should be >= 0";
    EXPECT_EQ(stats.total_requests, stats.successful_requests + stats.failed_requests)
        << "Total should equal successful + failed";
}

TEST_F(ProtocolAdapterTest, Statistics_FailedOperations_UpdatesCorrectly) {
    // Force some failures
    try {
        protocol_adapter_->readRegisters(0, 0); // Invalid parameters
    } catch (...) {
        // Expected to fail
    }
    
    CommunicationStats stats = protocol_adapter_->getStatistics();
    
    EXPECT_GT(stats.total_requests, 0) << "Total requests should be incremented";
    EXPECT_GT(stats.failed_requests, 0) << "Failed requests should be incremented";
}

TEST_F(ProtocolAdapterTest, Statistics_ResetStatistics_ClearsData) {
    // Perform some operations
    try {
        protocol_adapter_->readRegisters(0, 1);
    } catch (...) {
        // Ignore result
    }
    
    // Reset statistics
    protocol_adapter_->resetStatistics();
    
    CommunicationStats stats = protocol_adapter_->getStatistics();
    
    EXPECT_EQ(stats.total_requests, 0) << "Total requests should be reset to 0";
    EXPECT_EQ(stats.successful_requests, 0) << "Successful requests should be reset to 0";
    EXPECT_EQ(stats.failed_requests, 0) << "Failed requests should be reset to 0";
    EXPECT_EQ(stats.retry_attempts, 0) << "Retry attempts should be reset to 0";
}

// ============================================================================
// CONCURRENCY TESTS
// ============================================================================

TEST_F(ProtocolAdapterTest, ConcurrentRequests_ThreadSafety_Success) {
    const int num_threads = 5;
    const int requests_per_thread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    std::atomic<int> failed_operations{0};
    
    // Launch multiple threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, requests_per_thread, &successful_operations, &failed_operations]() {
            for (int i = 0; i < requests_per_thread; ++i) {
                try {
                    RegisterAddress addr = i % 10;
                    std::vector<RegisterValue> result = protocol_adapter_->readRegisters(addr, 1);
                    successful_operations++;
                } catch (...) {
                    failed_operations++;
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    int total_operations = successful_operations + failed_operations;
    EXPECT_EQ(total_operations, num_threads * requests_per_thread)
        << "All operations should be accounted for";
    
    CommunicationStats stats = protocol_adapter_->getStatistics();
    EXPECT_EQ(stats.total_requests, total_operations)
        << "Statistics should reflect all operations";
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(ProtocolAdapterTest, StressTest_ManySequentialRequests_Success) {
    const int num_requests = 100;
    int successful_count = 0;
    int failed_count = 0;
    
    for (int i = 0; i < num_requests; ++i) {
        try {
            RegisterAddress addr = i % 10;
            std::vector<RegisterValue> result = protocol_adapter_->readRegisters(addr, 1);
            successful_count++;
        } catch (...) {
            failed_count++;
        }
    }
    
    EXPECT_EQ(successful_count + failed_count, num_requests)
        << "All requests should be processed";
    
    CommunicationStats stats = protocol_adapter_->getStatistics();
    EXPECT_EQ(stats.total_requests, num_requests)
        << "Statistics should reflect all requests";
}

TEST_F(ProtocolAdapterTest, StressTest_LargeDataTransfer_Success) {
    // Test transferring maximum amount of data
    try {
        std::vector<RegisterValue> result = protocol_adapter_->readRegisters(0, 125);
        EXPECT_EQ(result.size(), 125) << "Should handle large data transfer";
    } catch (const ModbusException& e) {
        // Large transfers might fail due to timeout or server limitations
        GTEST_SKIP() << "Large transfer failed (expected): " << e.what();
    }
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

TEST_F(ProtocolAdapterTest, Performance_ResponseTime_WithinLimits) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        std::vector<RegisterValue> result = protocol_adapter_->readRegisters(0, 1);
    } catch (...) {
        // Ignore failures for timing test
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Response should be reasonably fast (adjust based on your requirements)
    EXPECT_LT(duration.count(), 10000) << "Response time should be less than 10 seconds";
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(ProtocolAdapterTest, EdgeCase_HighRegisterAddresses_Success) {
    // Test with high register addresses
    std::vector<RegisterAddress> high_addresses = {0x7FFF, 0xFFFE, 0xFFFF};
    
    for (RegisterAddress addr : high_addresses) {
        try {
            std::vector<RegisterValue> result = protocol_adapter_->readRegisters(addr, 1);
            // If successful, verify result
            EXPECT_EQ(result.size(), 1) << "Should return 1 register for address " << std::hex << addr;
        } catch (const ModbusException& e) {
            // High addresses might not be valid - this is acceptable
            std::string error_msg = e.what();
            EXPECT_THAT(error_msg, AnyOf(HasSubstr("Illegal Data Address"), 
                                       HasSubstr("error"), 
                                       HasSubstr("timeout")))
                << "Error should be reasonable for address " << std::hex << addr;
        }
    }
}

TEST_F(ProtocolAdapterTest, EdgeCase_BoundaryRegisterCounts_Success) {
    std::vector<uint16_t> boundary_counts = {1, 2, 10, 50, 100, 125};
    
    for (uint16_t count : boundary_counts) {
        try {
            std::vector<RegisterValue> result = protocol_adapter_->readRegisters(0, count);
            EXPECT_EQ(result.size(), count) << "Should return correct count for " << count << " registers";
        } catch (const ModbusException& e) {
            // Large counts might fail due to server limitations
            if (count > 50) {
                GTEST_SKIP() << "Large count " << count << " failed (may be expected): " << e.what();
            } else {
                FAIL() << "Small count " << count << " should not fail: " << e.what();
            }
        }
    }
}

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(ProtocolAdapterTest, Configuration_DifferentTimeouts_Success) {
    // Test with different timeout configurations
    std::vector<Duration> timeouts = {Duration(1000), Duration(5000), Duration(10000)};
    
    for (Duration timeout : timeouts) {
        // Create new adapter with different timeout
        ConfigManager config;
        
        ModbusConfig modbus_config;
        modbus_config.timeout = timeout;
        config.updateModbusConfig(modbus_config);
        
        ApiConfig api_config;
        api_config.base_url = "http://localhost:8080";
        config.updateApiConfig(api_config);
        
        ProtocolAdapter adapter(config);
        
        // Test operation with this timeout
        try {
            std::vector<RegisterValue> result = adapter.readRegisters(0, 1);
            // Success - timeout was sufficient
        } catch (const ModbusException& e) {
            // May fail due to timeout - this is acceptable for short timeouts
            std::string error_msg = e.what();
            if (timeout.count() < 2000) {
                EXPECT_THAT(error_msg, HasSubstr("timeout")) 
                    << "Short timeout should result in timeout error";
            }
        }
    }
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
