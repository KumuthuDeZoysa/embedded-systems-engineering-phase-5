#include "../include/modbus_frame.hpp"

// Standard Modbus RTU CRC16 (polynomial 0xA001)
uint16_t modbus_crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; ++j) {
            if (crc & 0x0001u) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}
