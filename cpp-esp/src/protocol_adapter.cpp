#include <Arduino.h>


#include "../include/protocol_adapter.hpp"
#include "../include/modbus_frame.hpp"
#include "../include/config_manager.hpp"
#include "../include/http_client.hpp"
#include "../include/logger.hpp"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Helper to convert a hex string to a byte array.
static int hexToBytes(const char* hexStr, uint8_t* outBytes, int maxLen) {
    int len = strlen(hexStr);
    if (len % 2 != 0) return -1; // Invalid hex string length
    int byteCount = 0;
    char byte_str[3] = {0};
    for (int i = 0; i < len && byteCount < maxLen; i += 2) {
        byte_str[0] = hexStr[i];
        byte_str[1] = hexStr[i+1];
        outBytes[byteCount++] = (uint8_t)strtol(byte_str, NULL, 16);
    }
    return byteCount;
}

// Simple JSON string field extractor for payloads like {"frame":"..."}
// Returns true if found and copies the value (without surrounding quotes) into outBuf.
static bool extractJsonStringField(const char* json, const char* key, char* outBuf, size_t outBufSize) {
    if (!json || !key || !outBuf) return false;
    // Build pattern: "key"
    char pat[128];
    size_t klen = strlen(key);
    if (klen + 3 >= sizeof(pat)) return false;
    pat[0] = '"';
    memcpy(pat + 1, key, klen);
    pat[1 + klen] = '"';
    pat[2 + klen] = '\0';
    const char* found = strstr(json, pat);
    if (!found) return false;
    // find colon after the key
    const char* colon = strchr(found + strlen(pat), ':');
    if (!colon) return false;
    // find first quote after colon
    const char* q1 = strchr(colon, '"');
    if (!q1) return false;
    const char* q2 = strchr(q1 + 1, '"');
    if (!q2) return false;
    size_t len = q2 - (q1 + 1);
    if (len + 1 > outBufSize) return false;
    memcpy(outBuf, q1 + 1, len);
    outBuf[len] = '\0';
    return true;
}

ProtocolAdapter::ProtocolAdapter(ConfigManager* config, EcoHttpClient* http_client)
    : config_(config), http_client_(http_client) {}

bool ProtocolAdapter::readRegisters(uint16_t start_address, uint16_t num_registers, uint16_t *values) {
    Logger::debug("Reading %d registers starting from address %d", num_registers, start_address);
    
    // Create Modbus frame for read
    uint8_t frame[8];
    frame[0] = config_->getModbusConfig().slave_address;
    frame[1] = 0x03; // Function code: Read Holding Registers
    frame[2] = (start_address >> 8) & 0xFF; // Start Address Hi
    frame[3] = start_address & 0xFF;        // Start Address Lo
    frame[4] = (num_registers >> 8) & 0xFF; // Quantity Hi
    frame[5] = num_registers & 0xFF;        // Quantity Lo
    uint16_t crc = modbus_crc16(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;
    ApiConfig api = config_->getApiConfig();
    
    char hex_frame_str[17]; // 8 bytes * 2 chars/byte + 1 null terminator
    for (int i = 0; i < 8; ++i) sprintf(hex_frame_str + i*2, "%02X", frame[i]);

    char json_payload[64];
    snprintf(json_payload, sizeof(json_payload), "{\"frame\": \"%s\"}", hex_frame_str);

    ModbusConfig mb_cfg = config_->getModbusConfig();
    int maxRetries = mb_cfg.max_retries ? mb_cfg.max_retries : 3;
    int attempt = 0;
    EcoHttpResponse resp;
    bool success = false;
    while (attempt < maxRetries && !success) {
    resp = http_client_->post(api.read_endpoint.c_str(), json_payload, strlen(json_payload), nullptr);
        success = resp.isSuccess();
        attempt++;
        if (!success && mb_cfg.retry_delay_ms) delay(mb_cfg.retry_delay_ms);
    }
    if (!success) {
        Logger::warn("ProtocolAdapter: HTTP request failed for readRegisters after %d retries.", maxRetries);
        return false;
    }

    // Parse the JSON response: {"frame": "1103040904002A2870"}
    char response_frame_hex[512];
    if (!extractJsonStringField(resp.body.c_str(), "frame", response_frame_hex, sizeof(response_frame_hex))) {
        Logger::warn("ProtocolAdapter: No 'frame' in JSON response.");
        return false;
    }

    // --- Full Modbus response frame validation ---
    uint8_t response_bytes[256];
    int response_len = hexToBytes(response_frame_hex, response_bytes, sizeof(response_bytes));

    if (response_len < 5) {
        Logger::warn("ProtocolAdapter: Response frame too short (%d bytes).", response_len);
        return false; // Must have at least slave addr, func, byte count, and 2-byte CRC
    }

    // 1. Validate CRC
    // CRC in Modbus frames is little-endian: low byte first, high byte second
    uint16_t received_crc = (uint16_t)response_bytes[response_len - 2] | ((uint16_t)response_bytes[response_len - 1] << 8);
    uint16_t calculated_crc = modbus_crc16(response_bytes, response_len - 2);
    if (received_crc != calculated_crc) {
        Logger::warn("ProtocolAdapter: CRC mismatch. Got 0x%04X, expected 0x%04X.", received_crc, calculated_crc);
        return false;
    }

    // 2. Validate Slave Address and Function Code
    if (response_bytes[0] != config_->getModbusConfig().slave_address) {
        Logger::warn("ProtocolAdapter: Slave address mismatch. Got %d, expected %d.", response_bytes[0], config_->getModbusConfig().slave_address);
        return false;
    }
    if (response_bytes[1] == (0x03 | 0x80)) { // Check for exception response
        Logger::warn("ProtocolAdapter: Inverter returned exception code 0x%02X.", response_bytes[2]);
        return false;
    }
    if (response_bytes[1] != 0x03) {
        Logger::warn("ProtocolAdapter: Function code mismatch. Got 0x%02X, expected 0x03.", response_bytes[1]);
        return false;
    }

    // 3. Validate Byte Count
    uint8_t byte_count = response_bytes[2];
    if (byte_count != num_registers * 2) {
        Logger::warn("ProtocolAdapter: Byte count mismatch. Got %d, expected %d.", byte_count, num_registers * 2);
        return false;
    }
    // --- Validation complete ---

    // Extract register values from the validated byte array
    for (int i = 0; i < num_registers; ++i) {
        int offset = 3 + (i * 2); // Data starts at byte 3
        values[i] = (response_bytes[offset] << 8) | response_bytes[offset + 1];
    }
    
    Logger::debug("Successfully read %d registers", num_registers);
    return true;
}

bool ProtocolAdapter::writeRegister(uint16_t register_address, uint16_t value) {
    Logger::debug("Writing value %d to register %d", value, register_address);
    
    // Create Modbus frame for write
    uint8_t frame[8];
    frame[0] = config_->getModbusConfig().slave_address;
    frame[1] = 0x06; // Function code: Write Single Register
    frame[2] = (register_address >> 8) & 0xFF; // Register Address Hi
    frame[3] = register_address & 0xFF;        // Register Address Lo
    frame[4] = (value >> 8) & 0xFF;            // Value Hi
    frame[5] = value & 0xFF;                   // Value Lo
    uint16_t crc = modbus_crc16(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;
    ApiConfig api = config_->getApiConfig();

    char hex_frame_str[17];
    for (int i = 0; i < 8; ++i) sprintf(hex_frame_str + i*2, "%02X", frame[i]);

    char json_payload[64];
    snprintf(json_payload, sizeof(json_payload), "{\"frame\": \"%s\"}", hex_frame_str);

    ModbusConfig mb_cfg = config_->getModbusConfig();
    int maxRetries = mb_cfg.max_retries ? mb_cfg.max_retries : 3;
    int attempt = 0;
    EcoHttpResponse resp;
    bool success = false;
    while (attempt < maxRetries && !success) {
    resp = http_client_->post(api.write_endpoint.c_str(), json_payload, strlen(json_payload), nullptr);
        success = resp.isSuccess();
        attempt++;
        if (!success && mb_cfg.retry_delay_ms) delay(mb_cfg.retry_delay_ms);
    }
    if (!success) {
        Logger::warn("ProtocolAdapter: HTTP request failed for writeRegister after %d retries.", maxRetries);
        return false;
    }

    // For a write, the response should be an echo of the request.
    char response_frame_hex[512];
    if (!extractJsonStringField(resp.body.c_str(), "frame", response_frame_hex, sizeof(response_frame_hex))) {
        Logger::warn("ProtocolAdapter: No 'frame' in JSON response for write operation.");
        return false;
    }

    // Convert response hex to bytes and validate similarly to readRegisters
    uint8_t response_bytes[256];
    int response_len = hexToBytes(response_frame_hex, response_bytes, sizeof(response_bytes));
    if (response_len < 5) {
        Logger::warn("ProtocolAdapter: Response frame too short (%d bytes) on write.", response_len);
        return false;
    }
    uint16_t received_crc = (uint16_t)response_bytes[response_len - 2] | ((uint16_t)response_bytes[response_len - 1] << 8);
    uint16_t calculated_crc = modbus_crc16(response_bytes, response_len - 2);
    if (received_crc != calculated_crc) {
        Logger::warn("ProtocolAdapter: CRC mismatch on write. Got 0x%04X, expected 0x%04X.", received_crc, calculated_crc);
        return false;
    }
    // Validate slave address and function
    if (response_bytes[0] != config_->getModbusConfig().slave_address) {
        Logger::warn("ProtocolAdapter: Slave address mismatch on write. Got %d, expected %d.", response_bytes[0], config_->getModbusConfig().slave_address);
        return false;
    }
    if (response_bytes[1] == (0x06 | 0x80)) {
        Logger::warn("ProtocolAdapter: Inverter returned exception on write operation.");
        return false;
    }
    // Success if response echoes request (same length and same bytes)
    if (response_len == 8) {
        bool match = true;
        for (int i = 0; i < 8; ++i) {
            uint8_t sent_byte = (uint8_t)strtol(hex_frame_str + i*2, nullptr, 16);
            if (sent_byte != response_bytes[i]) { match = false; break; }
        }
        if (match) {
            Logger::debug("Successfully wrote value %d to register %d", value, register_address);
            return true;
        }
    }
    return false;
}

bool ProtocolAdapter::testCommunication() {
    Logger::info("Testing communication with inverter SIM...");
    
    // Simple test: try to read one register
    uint16_t value = 0;
    bool success = readRegisters(0x00, 1, &value);
    
    if (success) {
        Logger::info("Communication test successful - read register 0: %d", value);
    } else {
        Logger::error("Communication test failed");
    }
    
    return success;
}
