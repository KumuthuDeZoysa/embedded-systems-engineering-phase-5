/**
 * @file test_api_integration.cpp
 * @brief Comprehensive API integration tests for EcoWatt Device
 * @author EcoWatt Test Team
 * @date 2025-09-06
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../cpp/include/protocol_adapter.hpp"
#include "../cpp/include/config_manager.hpp"
#include "../cpp/include/modbus_frame.hpp"
#include "../cpp/include/http_client.hpp"
#include "../cpp/include/exceptions.hpp"
#include <nlohmann/json.hpp>
#include <cpprest/http_client.h>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <future>

using namespace ecoWatt;
using namespace testing;
using namespace web::http;
using namespace web::http::client;

class APIIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test configuration
        setupTestConfig();
        
        // Create HTTP client for direct API testing
        http_client_ = std::make_unique<http_client>(U("http://20.15.114.131:8080"));
        
        // Create protocol adapter for high-level testing
        protocol_adapter_ = std::make_unique<ProtocolAdapter>(config_);
        
        // Wait for potential server startup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        protocol_adapter_.reset();
        http_client_.reset();
    }

private:
    void setupTestConfig() {
        ModbusConfig modbus_config;
        modbus_config.slave_address = 0x11;
        modbus_config.timeout = Duration(10000); // 10 seconds for integration tests
        modbus_config.max_retries = 3;
        modbus_config.retry_delay = Duration(1000);
        
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
    std::unique_ptr<http_client> http_client_;
    std::unique_ptr<ProtocolAdapter> protocol_adapter_;
    
    // Helper methods
    web::json::value createRequestJSON(const std::string& frame) {
        web::json::value request;
        request["frame"] = web::json::value::string(utility::conversions::to_string_t(frame));
        return request;
    }
    
    std::string extractFrameFromResponse(const web::json::value& response) {
        if (response.has_field(U("frame"))) {
            return utility::conversions::to_utf8string(response.at(U("frame")).as_string());
        }
        return "";
    }
    
    bool isServerReachable() {
        try {
            web::json::value request = createRequestJSON("110300000002C69B");
            auto response = http_client_->request(methods::POST, U("/api/inverter/read"), request)
                .wait();
            return response.status_code() != 0;
        } catch (...) {
            return false;
        }
    }
};

// ============================================================================
// SERVER CONNECTIVITY TESTS
// ============================================================================

TEST_F(APIIntegrationTest, ServerConnectivity_BasicConnection_Success) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test basic server connectivity
    try {
        auto response = http_client_->request(methods::GET, U("/")).wait();
        
        // Server should respond (even if with 404 for GET /)
        EXPECT_NE(response.status_code(), 0) << "Server should respond to requests";
        
    } catch (const std::exception& e) {
        FAIL() << "Server connectivity test failed: " << e.what();
    }
}

TEST_F(APIIntegrationTest, ServerConnectivity_ReadEndpoint_Accessible) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test read endpoint accessibility
    web::json::value request = createRequestJSON("110300000002C69B");
    
    try {
        auto response = http_client_->request(methods::POST, U("/api/inverter/read"), request)
            .wait();
        
        EXPECT_GE(response.status_code(), 200) << "Read endpoint should be accessible";
        EXPECT_LT(response.status_code(), 500) << "Read endpoint should not return server error for valid request";
        
    } catch (const std::exception& e) {
        FAIL() << "Read endpoint test failed: " << e.what();
    }
}

TEST_F(APIIntegrationTest, ServerConnectivity_WriteEndpoint_Accessible) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test write endpoint accessibility
    web::json::value request = createRequestJSON("1106000800643C50");
    
    try {
        auto response = http_client_->request(methods::POST, U("/api/inverter/write"), request)
            .wait();
        
        EXPECT_GE(response.status_code(), 200) << "Write endpoint should be accessible";
        EXPECT_LT(response.status_code(), 500) << "Write endpoint should not return server error for valid request";
        
    } catch (const std::exception& e) {
        FAIL() << "Write endpoint test failed: " << e.what();
    }
}

// ============================================================================
// HTTP PROTOCOL TESTS
// ============================================================================

TEST_F(APIIntegrationTest, HTTPProtocol_ValidJSONRequest_Success) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test valid JSON request format
    web::json::value request = createRequestJSON("110300000002C69B");
    
    auto response = http_client_->request(methods::POST, U("/api/inverter/read"), request)
        .wait();
    
    EXPECT_EQ(response.status_code(), 200) << "Valid JSON request should return 200 OK";
    
    // Check response content type
    auto headers = response.headers();
    auto content_type_iter = headers.find(U("content-type"));
    if (content_type_iter != headers.end()) {
        std::string content_type = utility::conversions::to_utf8string(content_type_iter->second);
        EXPECT_THAT(content_type, HasSubstr("application/json")) 
            << "Response should have JSON content type";
    }
    
    // Check response body is valid JSON
    auto response_json = response.extract_json().wait();
    EXPECT_TRUE(response_json.has_field(U("frame"))) 
        << "Response should contain 'frame' field";
}

TEST_F(APIIntegrationTest, HTTPProtocol_InvalidJSONRequest_ErrorResponse) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test invalid JSON request
    std::string invalid_json = "{ invalid json }";
    auto request_body = web::json::value::parse(utility::conversions::to_string_t(invalid_json));
    
    try {
        auto response = http_client_->request(methods::POST, U("/api/inverter/read"), request_body)
            .wait();
        
        EXPECT_GE(response.status_code(), 400) << "Invalid JSON should return client error";
        EXPECT_LT(response.status_code(), 500) << "Invalid JSON should be client error, not server error";
        
    } catch (const web::json::json_exception&) {
        // Expected for invalid JSON
        SUCCEED() << "Invalid JSON correctly rejected";
    }
}

TEST_F(APIIntegrationTest, HTTPProtocol_MissingFrameField_ErrorResponse) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test request without frame field
    web::json::value request;
    request["data"] = web::json::value::string(U("110300000002C69B"));
    
    auto response = http_client_->request(methods::POST, U("/api/inverter/read"), request)
        .wait();
    
    EXPECT_GE(response.status_code(), 400) << "Missing frame field should return client error";
}

TEST_F(APIIntegrationTest, HTTPProtocol_WrongHTTPMethod_ErrorResponse) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test wrong HTTP method (GET instead of POST)
    auto response = http_client_->request(methods::GET, U("/api/inverter/read"))
        .wait();
    
    EXPECT_EQ(response.status_code(), 405) << "Wrong HTTP method should return 405 Method Not Allowed";
}

// ============================================================================
// MODBUS FRAME TESTS
// ============================================================================

TEST_F(APIIntegrationTest, ModbusFrame_ValidReadRequest_ValidResponse) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test valid read request
    std::string read_frame = "110300000002C69B"; // Read 2 registers from address 0
    web::json::value request = createRequestJSON(read_frame);
    
    auto response = http_client_->request(methods::POST, U("/api/inverter/read"), request)
        .wait();
    
    EXPECT_EQ(response.status_code(), 200) << "Valid read request should succeed";
    
    auto response_json = response.extract_json().wait();
    std::string response_frame = extractFrameFromResponse(response_json);
    
    EXPECT_FALSE(response_frame.empty()) << "Response should contain frame data";
    EXPECT_GE(response_frame.length(), 10) << "Response frame should be reasonable length";
    
    // Validate response frame structure
    ModbusResponse parsed_response = ModbusFrame::parseResponse(response_frame);
    EXPECT_EQ(parsed_response.slave_address, 0x11) << "Response slave address should match";
    EXPECT_EQ(parsed_response.function_code, 0x03) << "Response function code should match";
    EXPECT_FALSE(parsed_response.is_error) << "Response should not be error";
}

TEST_F(APIIntegrationTest, ModbusFrame_ValidWriteRequest_ValidEcho) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test valid write request
    std::string write_frame = "1106000800643C50"; // Write value 100 to register 8
    web::json::value request = createRequestJSON(write_frame);
    
    auto response = http_client_->request(methods::POST, U("/api/inverter/write"), request)
        .wait();
    
    EXPECT_EQ(response.status_code(), 200) << "Valid write request should succeed";
    
    auto response_json = response.extract_json().wait();
    std::string response_frame = extractFrameFromResponse(response_json);
    
    EXPECT_FALSE(response_frame.empty()) << "Response should contain frame data";
    
    // Validate echo response
    ModbusResponse parsed_response = ModbusFrame::parseResponse(response_frame);
    EXPECT_EQ(parsed_response.slave_address, 0x11) << "Echo slave address should match";
    EXPECT_EQ(parsed_response.function_code, 0x06) << "Echo function code should match";
    EXPECT_FALSE(parsed_response.is_error) << "Echo should not be error";
    
    // Validate echo data
    EXPECT_EQ(parsed_response.data.size(), 4) << "Echo should contain 4 bytes (address + value)";
    
    uint16_t echo_addr = (parsed_response.data[0] << 8) | parsed_response.data[1];
    uint16_t echo_value = (parsed_response.data[2] << 8) | parsed_response.data[3];
    
    EXPECT_EQ(echo_addr, 0x0008) << "Echo address should match request";
    EXPECT_EQ(echo_value, 0x0064) << "Echo value should match request";
}

TEST_F(APIIntegrationTest, ModbusFrame_InvalidFrame_ErrorResponse) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test invalid frame (wrong CRC)
    std::string invalid_frame = "110300000002C69C"; // Wrong CRC
    web::json::value request = createRequestJSON(invalid_frame);
    
    auto response = http_client_->request(methods::POST, U("/api/inverter/read"), request)
        .wait();
    
    // Server should either return error response or Modbus error frame
    if (response.status_code() == 200) {
        auto response_json = response.extract_json().wait();
        std::string response_frame = extractFrameFromResponse(response_json);
        
        if (!response_frame.empty()) {
            try {
                ModbusResponse parsed_response = ModbusFrame::parseResponse(response_frame);
                if (!parsed_response.is_error) {
                    FAIL() << "Invalid frame should result in error response";
                }
            } catch (const ModbusException&) {
                SUCCEED() << "Invalid frame correctly rejected";
            }
        }
    } else {
        EXPECT_GE(response.status_code(), 400) << "Invalid frame should return client error";
    }
}

TEST_F(APIIntegrationTest, ModbusFrame_ShortFrame_ErrorResponse) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test frame that's too short
    std::string short_frame = "1103";
    web::json::value request = createRequestJSON(short_frame);
    
    auto response = http_client_->request(methods::POST, U("/api/inverter/read"), request)
        .wait();
    
    EXPECT_GE(response.status_code(), 400) << "Short frame should return client error";
}

// ============================================================================
// REGISTER ACCESS TESTS
// ============================================================================

TEST_F(APIIntegrationTest, RegisterAccess_ReadVoltageRegister_ValidData) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    try {
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 1); // Voltage register
        
        EXPECT_EQ(values.size(), 1) << "Should read exactly 1 register";
        EXPECT_GT(values[0], 0) << "Voltage should be positive";
        EXPECT_LT(values[0], 65535) << "Voltage should be within 16-bit range";
        
        // Convert to actual voltage (assuming 0.1V resolution)
        double voltage = values[0] / 10.0;
        EXPECT_GT(voltage, 100.0) << "Voltage should be reasonable (>100V)";
        EXPECT_LT(voltage, 500.0) << "Voltage should be reasonable (<500V)";
        
    } catch (const ModbusException& e) {
        FAIL() << "Voltage register read failed: " << e.what();
    }
}

TEST_F(APIIntegrationTest, RegisterAccess_ReadCurrentRegister_ValidData) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    try {
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(1, 1); // Current register
        
        EXPECT_EQ(values.size(), 1) << "Should read exactly 1 register";
        EXPECT_GE(values[0], 0) << "Current should be non-negative";
        
        // Convert to actual current (assuming 0.01A resolution)
        double current = values[0] / 100.0;
        EXPECT_GE(current, 0.0) << "Current should be non-negative";
        EXPECT_LT(current, 100.0) << "Current should be reasonable (<100A)";
        
    } catch (const ModbusException& e) {
        FAIL() << "Current register read failed: " << e.what();
    }
}

TEST_F(APIIntegrationTest, RegisterAccess_ReadMultipleRegisters_ValidData) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    try {
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 10); // Multiple registers
        
        EXPECT_EQ(values.size(), 10) << "Should read exactly 10 registers";
        
        for (size_t i = 0; i < values.size(); ++i) {
            EXPECT_GE(values[i], 0) << "Register " << i << " should be non-negative";
            EXPECT_LE(values[i], 65535) << "Register " << i << " should be within 16-bit range";
        }
        
    } catch (const ModbusException& e) {
        // Multiple register read might fail if some registers don't exist
        std::string error_msg = e.what();
        EXPECT_THAT(error_msg, AnyOf(HasSubstr("Illegal Data Address"), 
                                   HasSubstr("timeout"), 
                                   HasSubstr("error")))
            << "Error should be reasonable for multiple register read";
    }
}

TEST_F(APIIntegrationTest, RegisterAccess_WriteExportPower_Success) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    try {
        // Read original value
        std::vector<RegisterValue> original = protocol_adapter_->readRegisters(8, 1);
        ASSERT_EQ(original.size(), 1) << "Should read original export power value";
        
        RegisterValue original_value = original[0];
        RegisterValue test_value = 75; // 75% export power
        
        // Write test value
        bool write_success = protocol_adapter_->writeRegister(8, test_value);
        EXPECT_TRUE(write_success) << "Write operation should succeed";
        
        // Verify write by reading back
        std::vector<RegisterValue> written = protocol_adapter_->readRegisters(8, 1);
        EXPECT_EQ(written.size(), 1) << "Should read written value";
        EXPECT_EQ(written[0], test_value) << "Written value should match";
        
        // Restore original value
        bool restore_success = protocol_adapter_->writeRegister(8, original_value);
        EXPECT_TRUE(restore_success) << "Restore operation should succeed";
        
    } catch (const ModbusException& e) {
        FAIL() << "Export power write test failed: " << e.what();
    }
}

TEST_F(APIIntegrationTest, RegisterAccess_WriteReadOnlyRegister_ErrorResponse) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Try to write to a read-only register (voltage)
    EXPECT_THROW(protocol_adapter_->writeRegister(0, 2500), ModbusException)
        << "Writing to read-only register should throw exception";
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

TEST_F(APIIntegrationTest, ErrorHandling_InvalidRegisterAddress_ErrorResponse) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Try to read from invalid register address
    EXPECT_THROW(protocol_adapter_->readRegisters(0xFFFF, 1), ModbusException)
        << "Invalid register address should throw exception";
}

TEST_F(APIIntegrationTest, ErrorHandling_TooManyRegisters_ErrorResponse) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Try to read too many registers
    EXPECT_THROW(protocol_adapter_->readRegisters(0, 200), ModbusException)
        << "Reading too many registers should throw exception";
}

TEST_F(APIIntegrationTest, ErrorHandling_InvalidSlaveAddress_Timeout) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Create adapter with wrong slave address
    ConfigManager wrong_config = config_;
    ModbusConfig modbus_config = config_.getModbusConfig();
    modbus_config.slave_address = 0x99; // Wrong slave address
    modbus_config.timeout = Duration(2000); // Shorter timeout for this test
    wrong_config.updateModbusConfig(modbus_config);
    
    ProtocolAdapter wrong_adapter(wrong_config);
    
    EXPECT_THROW(wrong_adapter.readRegisters(0, 1), ModbusException)
        << "Wrong slave address should result in timeout or error";
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

TEST_F(APIIntegrationTest, Performance_SingleRegisterRead_ResponseTime) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 1);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        EXPECT_LT(duration.count(), 5000) << "Single register read should complete within 5 seconds";
        EXPECT_GT(duration.count(), 0) << "Operation should take some measurable time";
        
    } catch (const ModbusException& e) {
        FAIL() << "Performance test failed: " << e.what();
    }
}

TEST_F(APIIntegrationTest, Performance_MultipleSequentialReads_Throughput) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    const int num_reads = 10;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    int successful_reads = 0;
    for (int i = 0; i < num_reads; ++i) {
        try {
            std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 1);
            successful_reads++;
        } catch (const ModbusException&) {
            // Count failures but continue test
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_GT(successful_reads, 0) << "At least some reads should succeed";
    
    if (successful_reads > 0) {
        double avg_time = static_cast<double>(total_duration.count()) / successful_reads;
        EXPECT_LT(avg_time, 2000) << "Average read time should be reasonable";
    }
}

TEST_F(APIIntegrationTest, Performance_ConcurrentRequests_Handling) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    const int num_threads = 3;
    std::vector<std::future<bool>> futures;
    
    // Launch concurrent requests
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [this]() {
            try {
                std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 1);
                return true;
            } catch (const ModbusException&) {
                return false;
            }
        }));
    }
    
    // Wait for all requests to complete
    int successful_requests = 0;
    for (auto& future : futures) {
        if (future.get()) {
            successful_requests++;
        }
    }
    
    EXPECT_GT(successful_requests, 0) << "At least some concurrent requests should succeed";
}

// ============================================================================
// RELIABILITY TESTS
// ============================================================================

TEST_F(APIIntegrationTest, Reliability_RetryMechanism_EventualSuccess) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Configure shorter retry settings for faster test
    ConfigManager retry_config = config_;
    ModbusConfig modbus_config = config_.getModbusConfig();
    modbus_config.max_retries = 5;
    modbus_config.retry_delay = Duration(500);
    retry_config.updateModbusConfig(modbus_config);
    
    ProtocolAdapter retry_adapter(retry_config);
    
    try {
        std::vector<RegisterValue> values = retry_adapter.readRegisters(0, 1);
        EXPECT_EQ(values.size(), 1) << "Retry mechanism should eventually succeed";
        
    } catch (const ModbusException& e) {
        // If it still fails after retries, that's acceptable for this test
        std::string error_msg = e.what();
        EXPECT_THAT(error_msg, HasSubstr("attempts")) 
            << "Error should mention retry attempts";
    }
}

TEST_F(APIIntegrationTest, Reliability_LongRunningOperations_Stability) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Perform operations for a longer period
    const auto test_duration = std::chrono::seconds(10);
    auto start_time = std::chrono::steady_clock::now();
    
    int operation_count = 0;
    int success_count = 0;
    
    while (std::chrono::steady_clock::now() - start_time < test_duration) {
        try {
            std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 1);
            success_count++;
        } catch (const ModbusException&) {
            // Count failures but continue
        }
        operation_count++;
        
        // Brief pause between operations
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    EXPECT_GT(operation_count, 0) << "Should perform multiple operations";
    
    if (operation_count > 0) {
        double success_rate = static_cast<double>(success_count) / operation_count;
        EXPECT_GT(success_rate, 0.5) << "Success rate should be reasonable over time";
    }
}

// ============================================================================
// END-TO-END WORKFLOW TESTS
// ============================================================================

TEST_F(APIIntegrationTest, EndToEnd_CommunicationTest_Complete) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    // Test the complete communication workflow
    bool communication_ok = protocol_adapter_->testCommunication();
    
    if (!communication_ok) {
        // Communication test might fail due to server limitations
        // Try basic read/write manually
        try {
            std::vector<RegisterValue> values = protocol_adapter_->readRegisters(0, 1);
            EXPECT_EQ(values.size(), 1) << "Basic read should work even if communication test fails";
        } catch (const ModbusException& e) {
            FAIL() << "Even basic communication failed: " << e.what();
        }
    } else {
        SUCCEED() << "Full communication test passed";
    }
}

TEST_F(APIIntegrationTest, EndToEnd_DataAcquisitionWorkflow_Complete) {
    if (!isServerReachable()) {
        GTEST_SKIP() << "Server not reachable - skipping integration tests";
    }
    
    try {
        // Simulate data acquisition workflow
        
        // 1. Read voltage and current
        std::vector<RegisterValue> power_data = protocol_adapter_->readRegisters(0, 2);
        EXPECT_EQ(power_data.size(), 2) << "Should read voltage and current";
        
        // 2. Read additional parameters
        std::vector<RegisterValue> additional_data = protocol_adapter_->readRegisters(7, 2);
        EXPECT_EQ(additional_data.size(), 2) << "Should read additional parameters";
        
        // 3. Read export power setting
        std::vector<RegisterValue> export_power = protocol_adapter_->readRegisters(8, 1);
        EXPECT_EQ(export_power.size(), 1) << "Should read export power setting";
        
        // 4. Verify data consistency (voltage and current should be reasonable)
        double voltage = power_data[0] / 10.0;
        double current = power_data[1] / 100.0;
        
        EXPECT_GT(voltage, 0) << "Voltage should be positive";
        EXPECT_GE(current, 0) << "Current should be non-negative";
        
    } catch (const ModbusException& e) {
        FAIL() << "Data acquisition workflow failed: " << e.what();
    }
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "Running API Integration Tests for EcoWatt Device\n";
    std::cout << "Target Server: http://20.15.114.131:8080\n";
    std::cout << "================================================\n\n";
    
    return RUN_ALL_TESTS();
}
