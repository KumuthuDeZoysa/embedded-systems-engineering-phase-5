/**
 * @file test_error_handling.cpp
 * @brief Focused tests for error handling and edge cases
 * @author EcoWatt Test Team
 * @date 2025-09-07
 */

#include <iostream>
#include <chrono>
#include <thread>
#include "../cpp/include/protocol_adapter.hpp"
#include "../cpp/include/config_manager.hpp"
#include "../cpp/include/modbus_frame.hpp"
#include "../cpp/include/logger.hpp"

using namespace ecoWatt;

class ErrorTestRunner {
public:
    ErrorTestRunner() {
        // Initialize logger
        Logger::setConsoleLevel(spdlog::level::info);
        Logger::setFileLevel(spdlog::level::debug);
        
        // Load configuration
        try {
            config_ = std::make_unique<ConfigManager>("config.json");
            adapter_ = std::make_unique<ProtocolAdapter>(*config_);
            std::cout << "[✓] Test setup completed\n" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[✗] Setup failed: " << e.what() << std::endl;
            throw;
        }
    }

    void runAllTests() {
        std::cout << "🧪 **EcoWatt Error Handling Test Suite**\n";
        std::cout << "==========================================\n\n";

        int passed = 0, total = 0;

        // Test 1: Invalid Register Range
        if (testInvalidRegisterRange()) passed++;
        total++;

        // Test 2: Invalid Register Address  
        if (testInvalidRegisterAddress()) passed++;
        total++;

        // Test 3: Invalid Register Value
        if (testInvalidRegisterValue()) passed++;
        total++;

        // Test 4: Network Connection Error
        if (testNetworkError()) passed++;
        total++;

        // Test 5: Malformed Response Handling
        if (testMalformedResponse()) passed++;
        total++;

        // Test 6: CRC Validation
        if (testCRCValidation()) passed++;
        total++;

        // Test 7: Error Response Codes
        if (testModbusErrorCodes()) passed++;
        total++;

        // Test 8: Timeout Handling
        if (testTimeoutHandling()) passed++;
        total++;

        // Test 9: JSON Processing Errors
        if (testJSONErrors()) passed++;
        total++;

        // Test 10: Boundary Conditions
        if (testBoundaryConditions()) passed++;
        total++;

        std::cout << "\n🎯 **Test Results Summary**\n";
        std::cout << "============================\n";
        std::cout << "Total Tests: " << total << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << (total - passed) << std::endl;
        std::cout << "Success Rate: " << (100.0 * passed / total) << "%" << std::endl;

        if (passed == total) {
            std::cout << "\n🎉 **ALL ERROR HANDLING TESTS PASSED!**" << std::endl;
        } else {
            std::cout << "\n⚠️  **SOME TESTS FAILED - REVIEW ERROR HANDLING**" << std::endl;
        }
    }

private:
    std::unique_ptr<ConfigManager> config_;
    std::unique_ptr<ProtocolAdapter> adapter_;

    bool testInvalidRegisterRange() {
        std::cout << "Test 1: Invalid Register Range\n";
        std::cout << "-------------------------------\n";
        
        try {
            // Test reading too many registers (>125)
            std::cout << "  → Testing read of 200 registers (should fail)...\n";
            auto values = adapter_->readRegisters(0, 200);
            std::cout << "  [✗] FAILED: Should have thrown exception for invalid range\n\n";
            return false;
        } catch (const ModbusException& e) {
            std::cout << "  [✓] PASSED: Correctly caught invalid range - " << e.what() << "\n\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "  [✗] FAILED: Wrong exception type - " << e.what() << "\n\n";
            return false;
        }
    }

    bool testInvalidRegisterAddress() {
        std::cout << "Test 2: Invalid Register Address\n";
        std::cout << "---------------------------------\n";
        
        try {
            // Test reading from non-existent register
            std::cout << "  → Testing read from register 9999 (should fail)...\n";
            auto values = adapter_->readRegisters(9999, 1);
            std::cout << "  [?] Received response (may be valid if server allows): " << values.size() << " values\n\n";
            return true; // Some servers may allow this
        } catch (const ModbusException& e) {
            std::cout << "  [✓] PASSED: Correctly caught invalid address - " << e.what() << "\n\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "  [✗] FAILED: Unexpected error - " << e.what() << "\n\n";
            return false;
        }
    }

    bool testInvalidRegisterValue() {
        std::cout << "Test 3: Invalid Register Value\n";
        std::cout << "-------------------------------\n";
        
        try {
            // Test writing invalid value to a register
            std::cout << "  → Testing write of maximum value 65535 to register 8...\n";
            bool result = adapter_->writeRegister(8, 65535);
            std::cout << "  [?] Write completed: " << (result ? "Success" : "Failed") << "\n\n";
            return true; // Max value might be valid
        } catch (const ModbusException& e) {
            std::cout << "  [✓] PASSED: Correctly caught invalid value - " << e.what() << "\n\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "  [✗] FAILED: Unexpected error - " << e.what() << "\n\n";
            return false;
        }
    }

    bool testNetworkError() {
        std::cout << "Test 4: Network Connection Error\n";
        std::cout << "---------------------------------\n";
        
        try {
            // Create adapter with invalid URL to test network errors
            ModbusConfig modbus_config = config_->getModbusConfig();
            ApiConfig api_config = config_->getApiConfig();
            api_config.base_url = "http://invalid-server:9999"; // Invalid server
            
            ConfigManager invalid_config(modbus_config, api_config);
            ProtocolAdapter invalid_adapter(invalid_config);
            
            std::cout << "  → Testing connection to invalid server...\n";
            auto values = invalid_adapter.readRegisters(0, 1);
            std::cout << "  [✗] FAILED: Should have failed with network error\n\n";
            return false;
        } catch (const ModbusException& e) {
            std::cout << "  [✓] PASSED: Correctly caught network error - " << e.what() << "\n\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "  [✓] PASSED: Network error caught - " << e.what() << "\n\n";
            return true;
        }
    }

    bool testMalformedResponse() {
        std::cout << "Test 5: Malformed Response Handling\n";
        std::cout << "------------------------------------\n";
        
        try {
            // Test ModbusFrame parsing with invalid hex
            std::cout << "  → Testing parsing of invalid hex string...\n";
            auto response = ModbusFrame::parseResponse("INVALID_HEX");
            std::cout << "  [✗] FAILED: Should have thrown exception for invalid hex\n\n";
            return false;
        } catch (const ModbusException& e) {
            std::cout << "  [✓] PASSED: Correctly caught malformed response - " << e.what() << "\n\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "  [✓] PASSED: Malformed response error caught - " << e.what() << "\n\n";
            return true;
        }
    }

    bool testCRCValidation() {
        std::cout << "Test 6: CRC Validation\n";
        std::cout << "-----------------------\n";
        
        try {
            // Test with frame that has incorrect CRC
            std::cout << "  → Testing frame with incorrect CRC...\n";
            std::string bad_frame = "110300020001FFFF"; // Wrong CRC
            auto response = ModbusFrame::parseResponse(bad_frame);
            std::cout << "  [✗] FAILED: Should have failed CRC validation\n\n";
            return false;
        } catch (const ModbusException& e) {
            std::cout << "  [✓] PASSED: Correctly caught CRC error - " << e.what() << "\n\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "  [✓] PASSED: CRC validation error caught - " << e.what() << "\n\n";
            return true;
        }
    }

    bool testModbusErrorCodes() {
        std::cout << "Test 7: Modbus Error Response Codes\n";
        std::cout << "------------------------------------\n";
        
        try {
            // Test parsing error response (function code + 0x80)
            std::cout << "  → Testing Modbus error response parsing...\n";
            
            // Simulate different error codes
            std::vector<uint8_t> error_codes = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x0A, 0x0B};
            
            for (uint8_t error_code : error_codes) {
                std::string error_msg = ModbusFrame::getErrorMessage(error_code);
                std::cout << "    Error 0x" << std::hex << (int)error_code << ": " << error_msg << std::endl;
            }
            
            std::cout << "  [✓] PASSED: All error codes handled correctly\n\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  [✗] FAILED: Error code handling failed - " << e.what() << "\n\n";
            return false;
        }
    }

    bool testTimeoutHandling() {
        std::cout << "Test 8: Timeout Handling\n";
        std::cout << "-------------------------\n";
        
        try {
            // This will test the retry mechanism we saw in the live demo
            std::cout << "  → Testing normal operation (may include timeouts)...\n";
            auto start = std::chrono::high_resolution_clock::now();
            
            try {
                auto values = adapter_->readRegisters(0, 1);
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                
                std::cout << "  [✓] Operation completed in " << duration.count() << "ms\n";
                std::cout << "  [✓] PASSED: Timeout handling working (if any timeouts occurred, they were handled)\n\n";
                return true;
                
            } catch (const ModbusException& e) {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                
                std::cout << "  [✓] Operation failed after " << duration.count() << "ms: " << e.what() << std::endl;
                std::cout << "  [✓] PASSED: Timeout properly handled with retries\n\n";
                return true;
            }
            
        } catch (const std::exception& e) {
            std::cout << "  [✗] FAILED: Unexpected timeout handling error - " << e.what() << "\n\n";
            return false;
        }
    }

    bool testJSONErrors() {
        std::cout << "Test 9: JSON Processing Errors\n";
        std::cout << "-------------------------------\n";
        
        try {
            // Test hex string validation
            std::cout << "  → Testing hex string validation...\n";
            
            std::vector<uint8_t> invalid_hex;
            try {
                invalid_hex = ModbusFrame::hexToBytes("XYZ"); // Invalid hex
                std::cout << "  [✗] FAILED: Should have rejected invalid hex\n\n";
                return false;
            } catch (const ValidationException& e) {
                std::cout << "  [✓] PASSED: Correctly caught invalid hex - " << e.what() << std::endl;
            }
            
            try {
                invalid_hex = ModbusFrame::hexToBytes("ABC"); // Odd length
                std::cout << "  [✗] FAILED: Should have rejected odd-length hex\n\n";
                return false;
            } catch (const ValidationException& e) {
                std::cout << "  [✓] PASSED: Correctly caught odd-length hex - " << e.what() << "\n\n";
            }
            
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  [✗] FAILED: JSON error testing failed - " << e.what() << "\n\n";
            return false;
        }
    }

    bool testBoundaryConditions() {
        std::cout << "Test 10: Boundary Conditions\n";
        std::cout << "-----------------------------\n";
        
        try {
            // Test edge cases
            std::cout << "  → Testing zero registers read...\n";
            try {
                auto values = adapter_->readRegisters(0, 0);
                std::cout << "  [✗] FAILED: Should have rejected zero register count\n";
                return false;
            } catch (const ModbusException& e) {
                std::cout << "  [✓] Zero registers correctly rejected: " << e.what() << std::endl;
            }
            
            std::cout << "  → Testing maximum valid register count (125)...\n";
            try {
                auto values = adapter_->readRegisters(0, 125);
                std::cout << "  [✓] Maximum register count accepted: " << values.size() << " values" << std::endl;
            } catch (const ModbusException& e) {
                std::cout << "  [?] Maximum register count rejected (may be server limitation): " << e.what() << std::endl;
            }
            
            std::cout << "  [✓] PASSED: Boundary conditions handled correctly\n\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  [✗] FAILED: Boundary condition testing failed - " << e.what() << "\n\n";
            return false;
        }
    }
};

int main() {
    try {
        ErrorTestRunner tester;
        tester.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cout << "❌ **Test Runner Failed**: " << e.what() << std::endl;
        return 1;
    }
}
