#pragma once
#include "config_manager.hpp"
#include "http_client.hpp"
#include "modbus_frame.hpp"
#include <stdint.h>

class ProtocolAdapter {
public:
    ProtocolAdapter(ConfigManager* config, EcoHttpClient* http_client);
    ~ProtocolAdapter() = default;

    // Read holding registers from inverter
    bool readRegisters(uint16_t start_address, uint16_t num_registers, uint16_t *values);

    // Write single register to inverter
    bool writeRegister(uint16_t register_address, uint16_t value);

    // Test communication with inverter
    bool testCommunication();

private:
    ConfigManager* config_;
    EcoHttpClient* http_client_;
};
