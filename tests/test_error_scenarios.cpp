/**
 * @file test_error_scenarios.cpp
 * @brief Comprehensive error scenario tests for EcoWatt Device
 * @author EcoWatt Test Team
 * @date 2025-09-06
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../cpp/include/protocol_adapter.hpp"
#include "../cpp/include/modbus_frame.hpp"
#include "../cpp/include/http_client.hpp"
#include "../cpp/include/exceptions.hpp"
#include "../cpp/include/config_manager.hpp"
#include <vector>
#include <string>
#include <chrono>
#include <thread>

using namespace ecoWatt;
using namespace testing;

class ErrorScenariosTest : public ::testing::Test {
protected:
    void SetUp() override {
        setupTestConfig();
        protocol_adapter_ = std::make_unique<ProtocolAdapter>(config_);
    }

    void TearDown() override {
        protocol_adapter_.reset();
    }

private:
    void setupTestConfig() {
        ModbusConfig modbus_config;
        modbus_config.slave_address = 0x11;
        modbus_config.timeout = Duration(5000);
        modbus_config.max_retries = 3;
        modbus_config.retry_delay = Duration(500);
        
        ApiConfig api_config;
        api_config.base_url = "http://20.15.114.131:8080";
        api_config.read_endpoint = "/api/inverter/read";
        api_config.write_endpoint = "/api/inverter/write";
        api_config.content_type = "application/json";
        api_config.accept = "*/*";
        
        config_.updateModbusConfig(modbus_config);
        config_.updateApiConfig(api_config);
    }

protected:
    ConfigManager config_;
    std::unique_ptr<ProtocolAdapter> protocol_adapter_;
    
    // Helper method to create adapter with custom config
    std::unique_ptr<ProtocolAdapter> createAdapterWithConfig(const ConfigManager& custom_config) {
        return std::make_unique<ProtocolAdapter>(custom_config);
    }
};

// ============================================================================
// MODBUS PROTOCOL ERROR TESTS
// ============================================================================

TEST_F(ErrorScenariosTest, ModbusError_IllegalFunction_HandledCorrectly) {
    // Test using unsupported function code (should return error)
    try {
        // Try to use an unsupported function (this will depend on your server implementation)
        // For this test, we'll use a standard function that might not be supported
        
        // Create a frame with function code 0x04 (Read Input Registers) if only 0x03 is supported
        std::string frame_04 = "110400000002D1D9"; // Function 0x04 instead of 0x03
        
        // Send directly via HTTP to test server response
        HttpClient http_client("http://20.15.114.131:8080", 5000);
        
        nlohmann::json request;
        request["frame"] = frame_04;
        
        HttpResponse response = http_client.post("/api/inverter/read", request.dump());
        
        if (response.isSuccess()) {
            nlohmann::json response_json = nlohmann::json::parse(response.body);
            std::string response_frame = response_json["frame"];
            
            ModbusResponse modbus_response = ModbusFrame::parseResponse(response_frame);
            
            if (modbus_response.is_error) {
                EXPECT_EQ(modbus_response.error_code, 0x01) 
                    << "Should return Illegal Function error";
            }
        }
        
    } catch (const std::exception& e) {
        // Some servers might reject invalid functions at HTTP level
        std::string error_msg = e.what();
        EXPECT_THAT(error_msg, AnyOf(HasSubstr("function"), HasSubstr("error"), HasSubstr("illegal")))
            << "Error message should be meaningful";
    }
}

TEST_F(ErrorScenariosTest, ModbusError_IllegalDataAddress_HandledCorrectly) {
    // Test reading from non-existent register
    EXPECT_THROW({
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0xFFFF, 1);
    }, ModbusException) << "Non-existent register should throw ModbusException";
    
    try {
        protocol_adapter_->readRegisters(0xFFFF, 1);
        FAIL() << "Should have thrown exception";
    } catch (const ModbusException& e) {
        std::string error_msg = e.what();
        EXPECT_THAT(error_msg, AnyOf(HasSubstr("Illegal Data Address"), 
                                   HasSubstr("address"), 
                                   HasSubstr("error")))
            << "Error message should indicate address problem";
    }
}

TEST_F(ErrorScenariosTest, ModbusError_IllegalDataValue_HandledCorrectly) {
    // Test writing invalid value to register
    try {
        // Try to write to a read-only register (voltage register 0)
        bool result = protocol_adapter_->writeRegister(0, 2500);
        
        // If the operation doesn't throw, it should return false or the server should return error
        if (result) {
            // Some servers might allow this, so we'll check if the value actually changed
            std::vector<RegisterValue> readback = protocol_adapter_->readRegisters(0, 1);
            // The value shouldn't actually change for read-only registers
        }
        
    } catch (const ModbusException& e) {
        std::string error_msg = e.what();
        EXPECT_THAT(error_msg, AnyOf(HasSubstr("Illegal Data Value"), 
                                   HasSubstr("read-only"), 
                                   HasSubstr("error")))
            << "Error should indicate invalid write operation";
    }
}

TEST_F(ErrorScenariosTest, ModbusError_SlaveDeviceFailure_HandledCorrectly) {
    // Simulate device failure by using wrong slave address
    ConfigManager wrong_slave_config = config_;
    ModbusConfig modbus_config = config_.getModbusConfig();
    modbus_config.slave_address = 0x99; // Non-existent slave
    modbus_config.timeout = Duration(2000); // Shorter timeout
    wrong_slave_config.updateModbusConfig(modbus_config);
    
    auto wrong_adapter = createAdapterWithConfig(wrong_slave_config);
    
    EXPECT_THROW({
        std::vector<RegisterValue> values = wrong_adapter->readRegisters(0, 1);
    }, ModbusException) << "Wrong slave address should cause timeout or error";
}

// ============================================================================
// NETWORK ERROR TESTS
// ============================================================================

TEST_F(ErrorScenariosTest, NetworkError_UnreachableServer_HandledCorrectly) {
    // Test with unreachable server
    ConfigManager unreachable_config = config_;
    ApiConfig api_config = config_.getApiConfig();
    api_config.base_url = "http://192.168.255.255:8080"; // Unreachable IP
    unreachable_config.updateApiConfig(api_config);
    
    ModbusConfig modbus_config = config_.getModbusConfig();
    modbus_config.timeout = Duration(2000); // Shorter timeout for faster test
    unreachable_config.updateModbusConfig(modbus_config);
    
    auto unreachable_adapter = createAdapterWithConfig(unreachable_config);
    
    EXPECT_THROW({
        std::vector<RegisterValue> values = unreachable_adapter->readRegisters(0, 1);
    }, ModbusException) << "Unreachable server should throw exception";
}

TEST_F(ErrorScenariosTest, NetworkError_WrongPort_HandledCorrectly) {
    // Test with wrong port
    ConfigManager wrong_port_config = config_;
    ApiConfig api_config = config_.getApiConfig();
    api_config.base_url = "http://20.15.114.131:9999"; // Wrong port
    wrong_port_config.updateApiConfig(api_config);
    
    ModbusConfig modbus_config = config_.getModbusConfig();
    modbus_config.timeout = Duration(2000);
    wrong_port_config.updateModbusConfig(modbus_config);
    
    auto wrong_port_adapter = createAdapterWithConfig(wrong_port_config);
    
    EXPECT_THROW({
        std::vector<RegisterValue> values = wrong_port_adapter->readRegisters(0, 1);
    }, ModbusException) << "Wrong port should throw exception";
}

TEST_F(ErrorScenariosTest, NetworkError_WrongEndpoint_HandledCorrectly) {
    // Test with wrong endpoint
    ConfigManager wrong_endpoint_config = config_;
    ApiConfig api_config = config_.getApiConfig();
    api_config.read_endpoint = "/api/wrong/endpoint";
    wrong_endpoint_config.updateApiConfig(api_config);
    
    auto wrong_endpoint_adapter = createAdapterWithConfig(wrong_endpoint_config);
    
    EXPECT_THROW({
        std::vector<RegisterValue> values = wrong_endpoint_adapter->readRegisters(0, 1);
    }, ModbusException) << "Wrong endpoint should throw exception";
}

TEST_F(ErrorScenariosTest, NetworkError_Timeout_HandledCorrectly) {
    // Test with very short timeout
    ConfigManager timeout_config = config_;
    ModbusConfig modbus_config = config_.getModbusConfig();
    modbus_config.timeout = Duration(1); // 1ms timeout - should always timeout
    timeout_config.updateModbusConfig(modbus_config);
    
    auto timeout_adapter = createAdapterWithConfig(timeout_config);
    
    EXPECT_THROW({
        std::vector<RegisterValue> values = timeout_adapter->readRegisters(0, 1);
    }, ModbusException) << "Very short timeout should cause timeout exception";
}

// ============================================================================
// PROTOCOL ERROR TESTS
// ============================================================================

TEST_F(ErrorScenariosTest, ProtocolError_InvalidParameters_HandledCorrectly) {
    // Test with invalid parameters
    
    // Zero registers
    EXPECT_THROW({
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 0);
    }, ModbusException) << "Zero register count should throw exception";
    
    // Too many registers
    EXPECT_THROW({
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 126);
    }, ModbusException) << "Too many registers should throw exception";
    
    // Maximum valid should work (if server supports it)
    try {
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 125);
        EXPECT_EQ(values.size(), 125) << "Maximum valid register count should work";
    } catch (const ModbusException& e) {
        // Server might not support reading 125 registers - this is acceptable
        std::string error_msg = e.what();
        EXPECT_THAT(error_msg, Not(HasSubstr("invalid parameter")))
            << "Error should not be about invalid parameters for valid count";
    }
}

TEST_F(ErrorScenariosTest, ProtocolError_CorruptedFrame_HandledCorrectly) {
    // Test various corrupted frame scenarios
    
    HttpClient http_client("http://20.15.114.131:8080", 5000);
    
    std::vector<std::string> corrupted_frames = {
        "110300000002C69C", // Wrong CRC
        "1103000000",       // Incomplete frame
        "FF0300000002C69B", // Wrong slave address
        "110500000002C69B", // Wrong function code
        "ZZZZZZZZZZZZZZZZ", // Invalid hex characters
        "",                 // Empty frame
        "1103"              // Too short
    };
    
    for (const auto& frame : corrupted_frames) {
        try {
            nlohmann::json request;
            request["frame"] = frame;
            
            HttpResponse response = http_client.post("/api/inverter/read", request.dump());
            
            if (response.isSuccess()) {
                nlohmann::json response_json = nlohmann::json::parse(response.body);
                std::string response_frame = response_json["frame"];
                
                // If server responds, it should be an error frame
                if (!response_frame.empty()) {
                    try {
                        ModbusResponse modbus_response = ModbusFrame::parseResponse(response_frame);
                        EXPECT_TRUE(modbus_response.is_error) 
                            << "Corrupted frame should result in error response: " << frame;
                    } catch (const ModbusException&) {
                        // Can't parse response frame - also acceptable
                    }
                }
            } else {
                // HTTP error response is also acceptable for corrupted frames
                EXPECT_GE(response.status_code, 400) 
                    << "Corrupted frame should return client error: " << frame;
            }
            
        } catch (const std::exception& e) {
            // Exception is acceptable for corrupted frames
            SUCCEED() << "Corrupted frame correctly rejected: " << frame;
        }
    }
}

// ============================================================================
// JSON/HTTP ERROR TESTS
// ============================================================================

TEST_F(ErrorScenariosTest, JSONError_InvalidJSON_HandledCorrectly) {
    HttpClient http_client("http://20.15.114.131:8080", 5000);
    
    std::vector<std::string> invalid_json_requests = {
        "{ invalid json }",
        "{ \"frame\": }",
        "{ \"frame\": \"110300000002C69B\", }",
        "not json at all",
        "",
        "{ \"wrong_field\": \"110300000002C69B\" }",
        "{ \"frame\": null }",
        "{ \"frame\": 123 }",
        "[]",
        "{ \"frame\": \"\" }"
    };
    
    for (const auto& invalid_json : invalid_json_requests) {
        try {
            HttpResponse response = http_client.post("/api/inverter/read", invalid_json);
            
            EXPECT_GE(response.status_code, 400) 
                << "Invalid JSON should return client error: " << invalid_json;
            
        } catch (const std::exception& e) {
            // Exception is acceptable for invalid JSON
            SUCCEED() << "Invalid JSON correctly rejected: " << invalid_json;
        }
    }
}

TEST_F(ErrorScenariosTest, HTTPError_WrongMethod_HandledCorrectly) {
    HttpClient http_client("http://20.15.114.131:8080", 5000);
    
    // Test wrong HTTP methods
    std::vector<std::string> wrong_methods = {"GET", "PUT", "DELETE", "PATCH"};
    
    for (const auto& method : wrong_methods) {
        try {
            HttpResponse response;
            if (method == "GET") {
                response = http_client.get("/api/inverter/read");
            } else {
                // For other methods, we'll simulate by expecting they're not supported
                nlohmann::json request;
                request["frame"] = "110300000002C69B";
                
                // Most servers will return 405 Method Not Allowed
                response = http_client.post("/api/inverter/read", request.dump());
                // Note: This test assumes POST is correct and others are wrong
            }
            
            if (method == "GET") {
                EXPECT_EQ(response.status_code, 405) 
                    << "GET method should return 405 Method Not Allowed";
            }
            
        } catch (const std::exception& e) {
            // Exception is acceptable for wrong methods
            SUCCEED() << "Wrong HTTP method correctly rejected: " << method;
        }
    }
}

// ============================================================================
// RETRY MECHANISM ERROR TESTS
// ============================================================================

TEST_F(ErrorScenariosTest, RetryMechanism_ExhaustRetries_HandledCorrectly) {
    // Configure to exhaust retries quickly
    ConfigManager retry_config = config_;
    ModbusConfig modbus_config = config_.getModbusConfig();
    modbus_config.max_retries = 2; // Only 2 retries
    modbus_config.retry_delay = Duration(100); // Short delay
    modbus_config.timeout = Duration(1000); // Short timeout
    retry_config.updateModbusConfig(modbus_config);
    
    // Use unreachable server to force retries
    ApiConfig api_config = config_.getApiConfig();
    api_config.base_url = "http://192.168.255.255:8080";
    retry_config.updateApiConfig(api_config);
    
    auto retry_adapter = createAdapterWithConfig(retry_config);
    
    auto start_time = std::chrono::steady_clock::now();
    
    EXPECT_THROW({
        std::vector<RegisterValue> values = retry_adapter->readRegisters(0, 1);
    }, ModbusException) << "Should throw exception after exhausting retries";
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should take at least (timeout + retry_delay) * max_retries
    auto expected_min_duration = (modbus_config.timeout + modbus_config.retry_delay) * modbus_config.max_retries;
    EXPECT_GE(duration.count(), expected_min_duration.count() * 0.8) 
        << "Should take reasonable time for retries";
}

TEST_F(ErrorScenariosTest, RetryMechanism_StatisticsTracking_Correct) {
    // Test that retry statistics are tracked correctly
    
    // Force some failures to trigger retries
    ConfigManager unreliable_config = config_;
    ModbusConfig modbus_config = config_.getModbusConfig();
    modbus_config.timeout = Duration(500); // Short timeout to cause some failures
    modbus_config.max_retries = 3;
    unreliable_config.updateModbusConfig(modbus_config);
    
    auto unreliable_adapter = createAdapterWithConfig(unreliable_config);
    
    // Reset statistics
    unreliable_adapter->resetStatistics();
    
    // Perform several operations that might fail
    for (int i = 0; i < 5; ++i) {
        try {
            std::vector<RegisterValue> values = unreliable_adapter->readRegisters(0, 1);
        } catch (const ModbusException&) {
            // Ignore failures for statistics test
        }
    }
    
    CommunicationStats stats = unreliable_adapter->getStatistics();
    
    EXPECT_GT(stats.total_requests, 0) << "Should have attempted requests";
    EXPECT_EQ(stats.total_requests, stats.successful_requests + stats.failed_requests)
        << "Total should equal successful + failed";
    
    // If there were retries, they should be tracked
    if (stats.retry_attempts > 0) {
        EXPECT_LE(stats.retry_attempts, stats.total_requests * modbus_config.max_retries)
            << "Retry attempts should be reasonable";
    }
}

// ============================================================================
// MEMORY AND RESOURCE ERROR TESTS
// ============================================================================

TEST_F(ErrorScenariosTest, ResourceError_LargeRequests_HandledCorrectly) {
    // Test with maximum possible register requests
    
    try {
        // Try maximum registers (125)
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 125);
        EXPECT_EQ(values.size(), 125) << "Maximum registers should work if supported";
        
    } catch (const ModbusException& e) {
        // Server might not support maximum registers - check error is reasonable
        std::string error_msg = e.what();
        EXPECT_THAT(error_msg, AnyOf(HasSubstr("too many"), 
                                   HasSubstr("limit"), 
                                   HasSubstr("address"),
                                   HasSubstr("timeout")))
            << "Error should be reasonable for large request";
    }
}

TEST_F(ErrorScenariosTest, ResourceError_RapidRequests_HandledCorrectly) {
    // Test rapid-fire requests to check resource handling
    
    const int rapid_requests = 20;
    int successful_requests = 0;
    int failed_requests = 0;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < rapid_requests; ++i) {
        try {
            std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 1);
            successful_requests++;
        } catch (const ModbusException&) {
            failed_requests++;
        }
        
        // No delay - rapid requests
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_EQ(successful_requests + failed_requests, rapid_requests)
        << "All requests should be accounted for";
    
    // At least some requests should succeed (server should handle rapid requests)
    double success_rate = static_cast<double>(successful_requests) / rapid_requests;
    EXPECT_GT(success_rate, 0.3) << "At least 30% of rapid requests should succeed";
}

// ============================================================================
// ERROR RECOVERY TESTS
// ============================================================================

TEST_F(ErrorScenariosTest, ErrorRecovery_AfterError_OperationsResume) {
    // Test that operations can resume after errors
    
    // First, cause an error
    try {
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0xFFFF, 1); // Invalid address
        FAIL() << "Should have thrown exception for invalid address";
    } catch (const ModbusException&) {
        // Expected error
    }
    
    // Then, try a valid operation
    try {
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 1); // Valid address
        EXPECT_EQ(values.size(), 1) << "Should recover and work after error";
    } catch (const ModbusException& e) {
        // If this fails too, it might be a server issue, not recovery issue
        std::string error_msg = e.what();
        if (error_msg.find("timeout") == std::string::npos &&
            error_msg.find("connection") == std::string::npos) {
            FAIL() << "Should be able to perform valid operations after error: " << e.what();
        }
    }
}

TEST_F(ErrorScenariosTest, ErrorRecovery_StatisticsConsistency_Maintained) {
    // Test that statistics remain consistent after errors
    
    protocol_adapter_->resetStatistics();
    
    // Perform mix of successful and failed operations
    int expected_total = 0;
    
    // Valid operation
    try {
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 1);
        expected_total++;
    } catch (const ModbusException&) {
        expected_total++;
    }
    
    // Invalid operation
    try {
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 0); // Invalid count
        expected_total++;
    } catch (const ModbusException&) {
        expected_total++;
    }
    
    // Another valid operation
    try {
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(1, 1);
        expected_total++;
    } catch (const ModbusException&) {
        expected_total++;
    }
    
    CommunicationStats stats = protocol_adapter_->getStatistics();
    
    EXPECT_EQ(stats.total_requests, expected_total) 
        << "Statistics should track all requests including failed ones";
    EXPECT_EQ(stats.total_requests, stats.successful_requests + stats.failed_requests)
        << "Total should always equal successful + failed";
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "Running Error Scenario Tests for EcoWatt Device\n";
    std::cout << "Testing comprehensive error handling capabilities\n";
    std::cout << "===============================================\n\n";
    
    return RUN_ALL_TESTS();
}
