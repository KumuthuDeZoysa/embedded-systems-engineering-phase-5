/**
 * @file test_modbus_frame.cpp
 * @brief Comprehensive tests for ModbusFrame class
 * @author EcoWatt Test Team
 * @date 2025-09-06
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../cpp/include/modbus_frame.hpp"
#include "../cpp/include/exceptions.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

using namespace ecoWatt;
using namespace testing;

class ModbusFrameTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common test setup
    }

    void TearDown() override {
        // Common test cleanup
    }

    // Helper function to validate hex string format
    bool isValidHexString(const std::string& hex) {
        if (hex.length() % 2 != 0) return false;
        for (char c : hex) {
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// CREATE READ FRAME TESTS
// ============================================================================

TEST_F(ModbusFrameTest, CreateReadFrame_ValidParameters_Success) {
    // Test creating a read frame with valid parameters
    SlaveAddress slave = 0x11;
    RegisterAddress start = 0x0000;
    uint16_t count = 2;

    std::string frame = ModbusFrame::createReadFrame(slave, start, count);
    
    EXPECT_FALSE(frame.empty()) << "Frame should not be empty";
    EXPECT_TRUE(isValidHexString(frame)) << "Frame should be valid hex string";
    EXPECT_EQ(frame.length(), 16) << "Read frame should be 8 bytes (16 hex chars)";
    
    // Verify frame structure
    EXPECT_EQ(frame.substr(0, 2), "11") << "Slave address should be 0x11";
    EXPECT_EQ(frame.substr(2, 2), "03") << "Function code should be 0x03";
    EXPECT_EQ(frame.substr(4, 4), "0000") << "Start address should be 0x0000";
    EXPECT_EQ(frame.substr(8, 4), "0002") << "Register count should be 0x0002";
}

TEST_F(ModbusFrameTest, CreateReadFrame_DifferentSlaveAddresses_Success) {
    std::vector<SlaveAddress> slaves = {1, 17, 247, 255};
    
    for (SlaveAddress slave : slaves) {
        std::string frame = ModbusFrame::createReadFrame(slave, 0, 1);
        
        EXPECT_FALSE(frame.empty()) << "Frame should not be empty for slave " << (int)slave;
        EXPECT_TRUE(isValidHexString(frame)) << "Frame should be valid hex for slave " << (int)slave;
        
        // Verify slave address in frame
        std::string expected_slave = "";
        if (slave < 16) expected_slave = "0";
        expected_slave += std::to_string(slave);
        if (slave >= 10) {
            std::stringstream ss;
            ss << std::hex << std::uppercase << (int)slave;
            expected_slave = ss.str();
            if (expected_slave.length() == 1) expected_slave = "0" + expected_slave;
        }
        
        EXPECT_EQ(frame.substr(0, 2), expected_slave) << "Slave address mismatch for " << (int)slave;
    }
}

TEST_F(ModbusFrameTest, CreateReadFrame_DifferentRegisterCounts_Success) {
    std::vector<uint16_t> counts = {1, 2, 5, 10, 50, 125};
    
    for (uint16_t count : counts) {
        std::string frame = ModbusFrame::createReadFrame(0x11, 0, count);
        
        EXPECT_FALSE(frame.empty()) << "Frame should not be empty for count " << count;
        EXPECT_TRUE(isValidHexString(frame)) << "Frame should be valid hex for count " << count;
        
        // Verify register count in frame (bytes 8-11, big-endian)
        uint16_t frame_count = 0;
        std::string count_hex = frame.substr(8, 4);
        frame_count = std::stoul(count_hex, nullptr, 16);
        
        EXPECT_EQ(frame_count, count) << "Register count mismatch for " << count;
    }
}

TEST_F(ModbusFrameTest, CreateReadFrame_DifferentStartAddresses_Success) {
    std::vector<RegisterAddress> addresses = {0x0000, 0x0001, 0x0010, 0x0100, 0x1000, 0xFFFF};
    
    for (RegisterAddress addr : addresses) {
        std::string frame = ModbusFrame::createReadFrame(0x11, addr, 1);
        
        EXPECT_FALSE(frame.empty()) << "Frame should not be empty for address " << std::hex << addr;
        EXPECT_TRUE(isValidHexString(frame)) << "Frame should be valid hex for address " << std::hex << addr;
        
        // Verify start address in frame (bytes 4-7, big-endian)
        std::string addr_hex = frame.substr(4, 4);
        RegisterAddress frame_addr = std::stoul(addr_hex, nullptr, 16);
        
        EXPECT_EQ(frame_addr, addr) << "Start address mismatch for " << std::hex << addr;
    }
}

TEST_F(ModbusFrameTest, CreateReadFrame_CRCValidation_Success) {
    std::string frame = ModbusFrame::createReadFrame(0x11, 0x0000, 0x0002);
    
    // Convert to bytes for CRC validation
    std::vector<uint8_t> bytes = ModbusFrame::hexToBytes(frame);
    
    EXPECT_TRUE(ModbusFrame::validateFrame(bytes)) << "CRC validation should pass";
}

// ============================================================================
// CREATE WRITE FRAME TESTS
// ============================================================================

TEST_F(ModbusFrameTest, CreateWriteFrame_ValidParameters_Success) {
    SlaveAddress slave = 0x11;
    RegisterAddress reg_addr = 0x0008;
    RegisterValue value = 0x0064;

    std::string frame = ModbusFrame::createWriteFrame(slave, reg_addr, value);
    
    EXPECT_FALSE(frame.empty()) << "Frame should not be empty";
    EXPECT_TRUE(isValidHexString(frame)) << "Frame should be valid hex string";
    EXPECT_EQ(frame.length(), 16) << "Write frame should be 8 bytes (16 hex chars)";
    
    // Verify frame structure
    EXPECT_EQ(frame.substr(0, 2), "11") << "Slave address should be 0x11";
    EXPECT_EQ(frame.substr(2, 2), "06") << "Function code should be 0x06";
    EXPECT_EQ(frame.substr(4, 4), "0008") << "Register address should be 0x0008";
    EXPECT_EQ(frame.substr(8, 4), "0064") << "Register value should be 0x0064";
}

TEST_F(ModbusFrameTest, CreateWriteFrame_DifferentValues_Success) {
    std::vector<RegisterValue> values = {0x0000, 0x0001, 0x0064, 0x03E8, 0x7FFF, 0xFFFF};
    
    for (RegisterValue value : values) {
        std::string frame = ModbusFrame::createWriteFrame(0x11, 0x0008, value);
        
        EXPECT_FALSE(frame.empty()) << "Frame should not be empty for value " << std::hex << value;
        EXPECT_TRUE(isValidHexString(frame)) << "Frame should be valid hex for value " << std::hex << value;
        
        // Verify register value in frame (bytes 8-11, big-endian)
        std::string value_hex = frame.substr(8, 4);
        RegisterValue frame_value = std::stoul(value_hex, nullptr, 16);
        
        EXPECT_EQ(frame_value, value) << "Register value mismatch for " << std::hex << value;
    }
}

TEST_F(ModbusFrameTest, CreateWriteFrame_CRCValidation_Success) {
    std::string frame = ModbusFrame::createWriteFrame(0x11, 0x0008, 0x0064);
    
    // Convert to bytes for CRC validation
    std::vector<uint8_t> bytes = ModbusFrame::hexToBytes(frame);
    
    EXPECT_TRUE(ModbusFrame::validateFrame(bytes)) << "CRC validation should pass";
}

// ============================================================================
// PARSE RESPONSE TESTS
// ============================================================================

TEST_F(ModbusFrameTest, ParseResponse_ValidReadResponse_Success) {
    // Build a valid read response dynamically (CRC appended little-endian)
    std::vector<uint8_t> base_bytes = {0x11, 0x03, 0x04, 0x09, 0xC4, 0x04, 0x4E};
    uint16_t crc = ModbusFrame::calculateCRC(base_bytes);
    base_bytes.push_back(static_cast<uint8_t>(crc & 0xFF));      // CRC Lo
    base_bytes.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF)); // CRC Hi
    std::string response_hex = ModbusFrame::bytesToHex(base_bytes);
    
    ModbusResponse response = ModbusFrame::parseResponse(response_hex);
    
    EXPECT_EQ(response.slave_address, 0x11) << "Slave address should be 0x11";
    EXPECT_EQ(response.function_code, 0x03) << "Function code should be 0x03";
    EXPECT_FALSE(response.is_error) << "Should not be an error response";
    EXPECT_EQ(response.data.size(), 4) << "Data should contain 4 bytes";
    
    // Verify data content
    EXPECT_EQ(response.data[0], 0x09) << "First data byte should be 0x09";
    EXPECT_EQ(response.data[1], 0xC4) << "Second data byte should be 0xC4";
    EXPECT_EQ(response.data[2], 0x04) << "Third data byte should be 0x04";
    EXPECT_EQ(response.data[3], 0x4E) << "Fourth data byte should be 0x4E";
}

TEST_F(ModbusFrameTest, ParseResponse_ValidWriteResponse_Success) {
    // Build a valid write response (echo) dynamically (CRC appended little-endian)
    std::vector<uint8_t> base_bytes = {0x11, 0x06, 0x00, 0x08, 0x00, 0x64};
    uint16_t crc = ModbusFrame::calculateCRC(base_bytes);
    base_bytes.push_back(static_cast<uint8_t>(crc & 0xFF));
    base_bytes.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    std::string response_hex = ModbusFrame::bytesToHex(base_bytes);
    
    ModbusResponse response = ModbusFrame::parseResponse(response_hex);
    
    EXPECT_EQ(response.slave_address, 0x11) << "Slave address should be 0x11";
    EXPECT_EQ(response.function_code, 0x06) << "Function code should be 0x06";
    EXPECT_FALSE(response.is_error) << "Should not be an error response";
    EXPECT_EQ(response.data.size(), 4) << "Data should contain 4 bytes (addr + value)";
    
    // Verify echo data
    uint16_t addr = (response.data[0] << 8) | response.data[1];
    uint16_t value = (response.data[2] << 8) | response.data[3];
    
    EXPECT_EQ(addr, 0x0008) << "Echoed address should be 0x0008";
    EXPECT_EQ(value, 0x0064) << "Echoed value should be 0x0064";
}

TEST_F(ModbusFrameTest, ParseResponse_ErrorResponse_Success) {
    // Build an error response dynamically (func=0x83, exception=0x02)
    std::vector<uint8_t> base_bytes = {0x11, static_cast<uint8_t>(0x80 | 0x03), 0x02};
    uint16_t crc = ModbusFrame::calculateCRC(base_bytes);
    base_bytes.push_back(static_cast<uint8_t>(crc & 0xFF));
    base_bytes.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    std::string response_hex = ModbusFrame::bytesToHex(base_bytes);
    
    ModbusResponse response = ModbusFrame::parseResponse(response_hex);
    
    EXPECT_EQ(response.slave_address, 0x11) << "Slave address should be 0x11";
    EXPECT_EQ(response.function_code, 0x03) << "Function code should be 0x03 (without error bit)";
    EXPECT_TRUE(response.is_error) << "Should be an error response";
    EXPECT_EQ(response.error_code, 0x02) << "Error code should be 0x02";
}

TEST_F(ModbusFrameTest, ParseResponse_EmptyFrame_ThrowsException) {
    EXPECT_THROW(ModbusFrame::parseResponse(""), ModbusException) 
        << "Empty frame should throw ModbusException";
}

TEST_F(ModbusFrameTest, ParseResponse_ShortFrame_ThrowsException) {
    EXPECT_THROW(ModbusFrame::parseResponse("1103"), ModbusException) 
        << "Short frame should throw ModbusException";
}

TEST_F(ModbusFrameTest, ParseResponse_InvalidHex_ThrowsException) {
    EXPECT_THROW(ModbusFrame::parseResponse("11G30409C4044EE95D"), ModbusException) 
        << "Invalid hex should throw ModbusException";
}

TEST_F(ModbusFrameTest, ParseResponse_InvalidCRC_ThrowsException) {
    // Valid frame with corrupted CRC
    std::string corrupted_frame = "11030409C4044EE95E"; // Last byte changed
    
    EXPECT_THROW(ModbusFrame::parseResponse(corrupted_frame), ModbusException) 
        << "Invalid CRC should throw ModbusException";
}

// ============================================================================
// CRC CALCULATION TESTS
// ============================================================================

TEST_F(ModbusFrameTest, CalculateCRC_KnownValues_Success) {
    // Verify CRC calculation yields consistent values and that appending CRC validates
    std::vector<uint8_t> data1 = {0x11, 0x03, 0x00, 0x00, 0x00, 0x02};
    uint16_t crc1 = ModbusFrame::calculateCRC(data1);
    EXPECT_NE(crc1, 0x0000) << "CRC should not be zero";
    // Build full frame and validate
    {
        std::vector<uint8_t> frame = data1;
        frame.push_back(static_cast<uint8_t>(crc1 & 0xFF));
        frame.push_back(static_cast<uint8_t>((crc1 >> 8) & 0xFF));
        EXPECT_TRUE(ModbusFrame::validateFrame(frame)) << "Frame with appended CRC should validate";
    }
    
    std::vector<uint8_t> data2 = {0x11, 0x06, 0x00, 0x08, 0x00, 0x64};
    uint16_t crc2 = ModbusFrame::calculateCRC(data2);
    EXPECT_NE(crc2, 0x0000) << "CRC should not be zero";
    {
        std::vector<uint8_t> frame = data2;
        frame.push_back(static_cast<uint8_t>(crc2 & 0xFF));
        frame.push_back(static_cast<uint8_t>((crc2 >> 8) & 0xFF));
        EXPECT_TRUE(ModbusFrame::validateFrame(frame)) << "Write frame CRC should validate";
    }
}

TEST_F(ModbusFrameTest, CalculateCRC_EmptyData_Success) {
    std::vector<uint8_t> empty_data = {};
    uint16_t crc = ModbusFrame::calculateCRC(empty_data);
    EXPECT_EQ(crc, 0xFFFF) << "CRC for empty data should be 0xFFFF";
}

TEST_F(ModbusFrameTest, CalculateCRC_SingleByte_Success) {
    std::vector<uint8_t> single_byte = {0x11};
    uint16_t crc = ModbusFrame::calculateCRC(single_byte);
    EXPECT_NE(crc, 0x0000) << "CRC for single byte should not be 0x0000";
    EXPECT_NE(crc, 0xFFFF) << "CRC for single byte should not be 0xFFFF";
}

// ============================================================================
// HEX CONVERSION TESTS
// ============================================================================

TEST_F(ModbusFrameTest, HexToBytes_ValidHex_Success) {
    std::string hex = "11030409C4044E";
    std::vector<uint8_t> expected = {0x11, 0x03, 0x04, 0x09, 0xC4, 0x04, 0x4E};
    
    std::vector<uint8_t> result = ModbusFrame::hexToBytes(hex);
    
    EXPECT_EQ(result, expected) << "Hex conversion should match expected bytes";
}

TEST_F(ModbusFrameTest, HexToBytes_LowerCaseHex_Success) {
    std::string hex = "11030409c4044e";
    std::vector<uint8_t> expected = {0x11, 0x03, 0x04, 0x09, 0xC4, 0x04, 0x4E};
    
    std::vector<uint8_t> result = ModbusFrame::hexToBytes(hex);
    
    EXPECT_EQ(result, expected) << "Lower case hex should work";
}

TEST_F(ModbusFrameTest, HexToBytes_OddLength_ThrowsException) {
    EXPECT_THROW(ModbusFrame::hexToBytes("11030"), ValidationException) 
        << "Odd length hex should throw ValidationException";
}

TEST_F(ModbusFrameTest, HexToBytes_InvalidCharacter_ThrowsException) {
    EXPECT_THROW(ModbusFrame::hexToBytes("1103G4"), ValidationException) 
        << "Invalid hex character should throw ValidationException";
}

TEST_F(ModbusFrameTest, BytesToHex_ValidBytes_Success) {
    std::vector<uint8_t> bytes = {0x11, 0x03, 0x04, 0x09, 0xC4, 0x04, 0x4E};
    std::string expected = "11030409C4044E";
    
    std::string result = ModbusFrame::bytesToHex(bytes);
    
    EXPECT_EQ(result, expected) << "Bytes to hex conversion should match expected";
}

TEST_F(ModbusFrameTest, BytesToHex_EmptyBytes_Success) {
    std::vector<uint8_t> empty_bytes = {};
    std::string result = ModbusFrame::bytesToHex(empty_bytes);
    
    EXPECT_EQ(result, "") << "Empty bytes should produce empty string";
}

TEST_F(ModbusFrameTest, BytesToHex_SingleByte_Success) {
    std::vector<uint8_t> single_byte = {0xFF};
    std::string result = ModbusFrame::bytesToHex(single_byte);
    
    EXPECT_EQ(result, "FF") << "Single byte should produce correct hex";
}

// ============================================================================
// FRAME VALIDATION TESTS
// ============================================================================

TEST_F(ModbusFrameTest, ValidateFrame_ValidFrame_Success) {
    // Valid read request frame
    std::vector<uint8_t> valid_frame = {0x11, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC6, 0x9B};
    
    EXPECT_TRUE(ModbusFrame::validateFrame(valid_frame)) << "Valid frame should pass validation";
}

TEST_F(ModbusFrameTest, ValidateFrame_InvalidCRC_Failure) {
    // Frame with wrong CRC
    std::vector<uint8_t> invalid_frame = {0x11, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC6, 0x9C};
    
    EXPECT_FALSE(ModbusFrame::validateFrame(invalid_frame)) << "Invalid CRC should fail validation";
}

TEST_F(ModbusFrameTest, ValidateFrame_ShortFrame_Failure) {
    // Frame too short
    std::vector<uint8_t> short_frame = {0x11, 0x03, 0x00, 0x00};
    
    EXPECT_FALSE(ModbusFrame::validateFrame(short_frame)) << "Short frame should fail validation";
}

TEST_F(ModbusFrameTest, ValidateFrame_MinimumLength_Success) {
    // Minimum valid frame (5 bytes)
    std::vector<uint8_t> min_frame = {0x11, 0x03, 0x00, 0x85, 0x65};
    
    // Calculate correct CRC
    std::vector<uint8_t> data_part = {0x11, 0x03, 0x00};
    uint16_t crc = ModbusFrame::calculateCRC(data_part);
    min_frame[3] = crc & 0xFF;
    min_frame[4] = (crc >> 8) & 0xFF;
    
    EXPECT_TRUE(ModbusFrame::validateFrame(min_frame)) << "Minimum length frame should pass validation";
}

// ============================================================================
// ERROR MESSAGE TESTS
// ============================================================================

TEST_F(ModbusFrameTest, GetErrorMessage_StandardCodes_Success) {
    EXPECT_EQ(ModbusFrame::getErrorMessage(0x01), "Illegal Function") << "Error code 0x01 message";
    EXPECT_EQ(ModbusFrame::getErrorMessage(0x02), "Illegal Data Address") << "Error code 0x02 message";
    EXPECT_EQ(ModbusFrame::getErrorMessage(0x03), "Illegal Data Value") << "Error code 0x03 message";
    EXPECT_EQ(ModbusFrame::getErrorMessage(0x04), "Slave Device Failure") << "Error code 0x04 message";
    EXPECT_EQ(ModbusFrame::getErrorMessage(0x05), "Acknowledge") << "Error code 0x05 message";
    EXPECT_EQ(ModbusFrame::getErrorMessage(0x06), "Slave Device Busy") << "Error code 0x06 message";
    EXPECT_EQ(ModbusFrame::getErrorMessage(0x08), "Memory Parity Error") << "Error code 0x08 message";
    EXPECT_EQ(ModbusFrame::getErrorMessage(0x0A), "Gateway Path Unavailable") << "Error code 0x0A message";
    EXPECT_EQ(ModbusFrame::getErrorMessage(0x0B), "Gateway Target Device Failed to Respond") << "Error code 0x0B message";
}

TEST_F(ModbusFrameTest, GetErrorMessage_UnknownCode_Success) {
    EXPECT_EQ(ModbusFrame::getErrorMessage(0xFF), "Unknown Error") << "Unknown error code message";
    EXPECT_EQ(ModbusFrame::getErrorMessage(0x99), "Unknown Error") << "Another unknown error code message";
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

TEST_F(ModbusFrameTest, RoundTrip_ReadFrameCreationAndParsing_Success) {
    // Create a read frame
    SlaveAddress slave = 0x11;
    RegisterAddress start = 0x0000;
    uint16_t count = 2;
    
    std::string request_frame = ModbusFrame::createReadFrame(slave, start, count);
    
    // Simulate a valid response (build dynamically)
    std::vector<uint8_t> base_bytes = {0x11, 0x03, 0x04, 0x09, 0xC4, 0x04, 0x4E};
    uint16_t crc = ModbusFrame::calculateCRC(base_bytes);
    base_bytes.push_back(static_cast<uint8_t>(crc & 0xFF));
    base_bytes.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    std::string response_frame = ModbusFrame::bytesToHex(base_bytes);
    
    // Parse the response
    ModbusResponse response = ModbusFrame::parseResponse(response_frame);
    
    EXPECT_EQ(response.slave_address, slave) << "Response slave should match request";
    EXPECT_EQ(response.function_code, 0x03) << "Response function should be read";
    EXPECT_FALSE(response.is_error) << "Response should not be error";
    EXPECT_EQ(response.data.size(), 4) << "Response should contain 4 bytes (2 registers)";
}

TEST_F(ModbusFrameTest, RoundTrip_WriteFrameCreationAndParsing_Success) {
    // Create a write frame
    SlaveAddress slave = 0x11;
    RegisterAddress reg_addr = 0x0008;
    RegisterValue value = 0x0064;
    
    std::string request_frame = ModbusFrame::createWriteFrame(slave, reg_addr, value);
    
    // Simulate a valid echo response (build dynamically)
    std::vector<uint8_t> base_bytes = {0x11, 0x06, 0x00, 0x08, 0x00, 0x64};
    uint16_t crc = ModbusFrame::calculateCRC(base_bytes);
    base_bytes.push_back(static_cast<uint8_t>(crc & 0xFF));
    base_bytes.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    std::string response_frame = ModbusFrame::bytesToHex(base_bytes);
    
    // Parse the response
    ModbusResponse response = ModbusFrame::parseResponse(response_frame);
    
    EXPECT_EQ(response.slave_address, slave) << "Response slave should match request";
    EXPECT_EQ(response.function_code, 0x06) << "Response function should be write";
    EXPECT_FALSE(response.is_error) << "Response should not be error";
    
    // Verify echo data
    uint16_t echo_addr = (response.data[0] << 8) | response.data[1];
    uint16_t echo_value = (response.data[2] << 8) | response.data[3];
    
    EXPECT_EQ(echo_addr, reg_addr) << "Echo address should match request";
    EXPECT_EQ(echo_value, value) << "Echo value should match request";
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(ModbusFrameTest, StressTest_MultipleFrameCreations_Success) {
    // Test creating many frames to ensure no memory leaks or performance issues
    for (int i = 0; i < 1000; i++) {
        SlaveAddress slave = static_cast<SlaveAddress>(i % 256);
        RegisterAddress start = static_cast<RegisterAddress>(i % 65536);
        uint16_t count = (i % 125) + 1;
        
        std::string frame = ModbusFrame::createReadFrame(slave, start, count);
        
        EXPECT_FALSE(frame.empty()) << "Frame " << i << " should not be empty";
        EXPECT_TRUE(isValidHexString(frame)) << "Frame " << i << " should be valid hex";
        
        // Validate CRC
        std::vector<uint8_t> bytes = ModbusFrame::hexToBytes(frame);
        EXPECT_TRUE(ModbusFrame::validateFrame(bytes)) << "Frame " << i << " should have valid CRC";
    }
}

TEST_F(ModbusFrameTest, StressTest_LargeFrameParsing_Success) {
    // Test parsing frames with maximum register count
    std::string large_response = "1103FA"; // Header for 125 registers (250 bytes)
    
    // Add 250 bytes of data (125 registers)
    for (int i = 0; i < 125; i++) {
        large_response += "1234"; // Mock register value
    }
    
    // Calculate and append CRC
    std::vector<uint8_t> frame_data = ModbusFrame::hexToBytes(large_response);
    uint16_t crc = ModbusFrame::calculateCRC(frame_data);
    
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (crc & 0xFF);
    ss << std::setw(2) << ((crc >> 8) & 0xFF);
    large_response += ss.str();
    
    // Parse the large response
    EXPECT_NO_THROW({
        ModbusResponse response = ModbusFrame::parseResponse(large_response);
        EXPECT_EQ(response.data.size(), 250) << "Large response should contain 250 bytes";
    }) << "Large frame parsing should not throw exception";
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(ModbusFrameTest, EdgeCase_MaximumValues_Success) {
    // Test with maximum values
    SlaveAddress max_slave = 255;
    RegisterAddress max_addr = 65535;
    RegisterValue max_value = 65535;
    uint16_t max_count = 125;
    
    // Test read frame with max values
    std::string read_frame = ModbusFrame::createReadFrame(max_slave, max_addr, max_count);
    EXPECT_FALSE(read_frame.empty()) << "Max read frame should not be empty";
    
    // Test write frame with max values
    std::string write_frame = ModbusFrame::createWriteFrame(max_slave, max_addr, max_value);
    EXPECT_FALSE(write_frame.empty()) << "Max write frame should not be empty";
    
    // Validate both frames
    EXPECT_TRUE(ModbusFrame::validateFrame(ModbusFrame::hexToBytes(read_frame))) 
        << "Max read frame should be valid";
    EXPECT_TRUE(ModbusFrame::validateFrame(ModbusFrame::hexToBytes(write_frame))) 
        << "Max write frame should be valid";
}

TEST_F(ModbusFrameTest, EdgeCase_MinimumValues_Success) {
    // Test with minimum values
    SlaveAddress min_slave = 1;
    RegisterAddress min_addr = 0;
    RegisterValue min_value = 0;
    uint16_t min_count = 1;
    
    // Test read frame with min values
    std::string read_frame = ModbusFrame::createReadFrame(min_slave, min_addr, min_count);
    EXPECT_FALSE(read_frame.empty()) << "Min read frame should not be empty";
    
    // Test write frame with min values
    std::string write_frame = ModbusFrame::createWriteFrame(min_slave, min_addr, min_value);
    EXPECT_FALSE(write_frame.empty()) << "Min write frame should not be empty";
    
    // Validate both frames
    EXPECT_TRUE(ModbusFrame::validateFrame(ModbusFrame::hexToBytes(read_frame))) 
        << "Min read frame should be valid";
    EXPECT_TRUE(ModbusFrame::validateFrame(ModbusFrame::hexToBytes(write_frame))) 
        << "Min write frame should be valid";
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
