#pragma once

#include <cstdint>

// CRC utility for Modbus (returns 16-bit CRC as specified by Modbus RTU)
// data: pointer to bytes
// length: number of bytes to include in CRC calculation
uint16_t modbus_crc16(const uint8_t *data, uint16_t length);
