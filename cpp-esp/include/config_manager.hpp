
#pragma once
#include <stdint.h>
#include <vector>
#include <string>
#include "config_update.hpp"

struct LoggingConfig {
    std::string log_level;
    std::string log_file;
    bool flush_on_write;
};

struct ModbusConfig {
    uint8_t slave_address;
    uint32_t timeout_ms;
    uint8_t max_retries;
    uint32_t retry_delay_ms;
};

struct ApiConfig {
    std::string inverter_base_url;
    std::string upload_base_url;
    std::string read_endpoint;
    std::string write_endpoint;
    std::string config_endpoint;
    std::string upload_endpoint;
    std::string api_key;
};

struct RegisterConfig {
    uint8_t addr;
    std::string name;
    std::string unit;
    float gain;
    std::string access;
};

struct AcquisitionConfig {
    uint32_t polling_interval_ms;
    std::vector<uint8_t> minimum_registers;
    bool background_polling;
};

class ConfigManager {
public:
    ConfigManager(const char* config_file = "/config/config.json");
    ~ConfigManager();

    std::string getDeviceId() const;
    ModbusConfig getModbusConfig() const;
    ApiConfig getApiConfig() const;
    RegisterConfig getRegisterConfig(uint8_t addr) const;
    AcquisitionConfig getAcquisitionConfig() const;
    LoggingConfig getLoggingConfig() const;
    
    // Runtime configuration update methods
    bool validateSamplingInterval(uint32_t interval_ms, std::string& reason) const;
    bool validateRegisters(const std::vector<uint8_t>& registers, std::string& reason) const;
    ConfigUpdateAck applyConfigUpdate(const ConfigUpdateRequest& request);
    
    // Persistence methods
    bool savePersistentConfig();
    bool loadPersistentConfig();
    
    // Runtime getters/setters for dynamic config
    void setSamplingInterval(uint32_t interval_ms);
    uint32_t getSamplingInterval() const;
    void setActiveRegisters(const std::vector<uint8_t>& registers);
    std::vector<uint8_t> getActiveRegisters() const;
    
    // Nonce management for idempotency
    bool isNonceProcessed(uint32_t nonce) const;
    void markNonceProcessed(uint32_t nonce);
    
    // Get validation rules
    ConfigValidationRules getValidationRules() const;

private:
    std::string device_id_;
    ModbusConfig modbus_config_;
    ApiConfig api_config_;
    std::vector<RegisterConfig> register_configs_;
    AcquisitionConfig acquisition_config_;
    LoggingConfig logging_config_;
    ConfigValidationRules validation_rules_;
    PersistentConfig persistent_config_;
    
    void loadConfig(const char* config_file);
    void initializeDefaults();
    uint32_t calculateChecksum(const PersistentConfig& config) const;
    bool verifyChecksum(const PersistentConfig& config) const;
    
    std::string getRegisterListString(const std::vector<uint8_t>& registers) const;
};
