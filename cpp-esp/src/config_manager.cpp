
#include "../include/config_manager.hpp"
#include "../include/logger.hpp"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

ConfigManager::ConfigManager(const char* config_file) {
    initializeDefaults();
    
    // Try to load persistent config from flash
    if (!loadPersistentConfig()) {
        Logger::info("No persistent config found, using defaults");
        // Initialize persistent config with defaults
        persistent_config_.version = 1;
        persistent_config_.sampling_interval_ms = acquisition_config_.polling_interval_ms;
        persistent_config_.registers = acquisition_config_.minimum_registers;
        persistent_config_.last_nonce = 0;
        persistent_config_.last_update_timestamp = 0;
        savePersistentConfig();
    } else {
        // Apply loaded persistent config
        acquisition_config_.polling_interval_ms = persistent_config_.sampling_interval_ms;
        acquisition_config_.minimum_registers = persistent_config_.registers;
        Logger::info("Loaded persistent config: interval=%u ms, registers=%u",
                     persistent_config_.sampling_interval_ms,
                     (unsigned)persistent_config_.registers.size());
    }
}

void ConfigManager::initializeDefaults() {
    // Default device ID
    device_id_ = "EcoWatt001";
    
    // Hardcoded Modbus config
    modbus_config_.slave_address = 17;
    modbus_config_.timeout_ms = 5000;
    modbus_config_.max_retries = 3;
    modbus_config_.retry_delay_ms = 1000;

    // Hardcoded API config (inverter data accessed via api_key only)
    api_config_.inverter_base_url = "http://20.15.114.131:8080";
    api_config_.upload_base_url = "http://10.52.180.183:8080";  // Cloud server for config/upload
    api_config_.read_endpoint = "/api/inverter/read";
    api_config_.write_endpoint = "/api/inverter/write";
    api_config_.config_endpoint = "/api/inverter/config";
    api_config_.upload_endpoint = "http://10.52.180.183:8080/api/upload";
    api_config_.api_key = "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFkOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExMw==";

    // Set correct gain values for each register
    register_configs_.clear();
    register_configs_.push_back({0, "Vac1_L1_Phase_voltage", "V", 10.0f, "Read"});
    register_configs_.push_back({1, "Iac1_L1_Phase_current", "A", 10.0f, "Read"});
    register_configs_.push_back({2, "Fac1_L1_Phase_frequency", "Hz", 100.0f, "Read"});
    register_configs_.push_back({3, "Vpv1_PV1_input_voltage", "V", 10.0f, "Read"});
    register_configs_.push_back({4, "Vpv2_PV2_input_voltage", "V", 10.0f, "Read"});
    register_configs_.push_back({5, "Ipv1_PV1_input_current", "A", 10.0f, "Read"});
    register_configs_.push_back({6, "Ipv2_PV2_input_current", "A", 10.0f, "Read"});
    register_configs_.push_back({7, "Inverter_internal_temperature", "°C", 10.0f, "Read"});
    register_configs_.push_back({8, "Export_power_percentage", "%", 1.0f, "Read/Write"});
    register_configs_.push_back({9, "Pac_L_Inverter_output_power", "W", 1.0f, "Read"});


    // Hardcoded acquisition config
    acquisition_config_.polling_interval_ms = 5000; // 5 seconds
    acquisition_config_.minimum_registers = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    acquisition_config_.background_polling = true;

    // Hardcoded logging config
    logging_config_.log_level = "DEBUG";
    logging_config_.log_file = "/logs/main.log";
    logging_config_.flush_on_write = true;
    
    // Initialize validation rules with defaults
    validation_rules_.min_sampling_interval_ms = 1000;    // 1 second
    validation_rules_.max_sampling_interval_ms = 300000;  // 5 minutes
    validation_rules_.min_register_addr = 0;
    validation_rules_.max_register_addr = 9;
    validation_rules_.min_register_count = 1;
    validation_rules_.max_register_count = 10;
    validation_rules_.max_nonce_age_ms = 300000;  // 5 minutes
}

ConfigManager::~ConfigManager() {}

std::string ConfigManager::getDeviceId() const { return device_id_; }

ModbusConfig ConfigManager::getModbusConfig() const { return modbus_config_; }
ApiConfig ConfigManager::getApiConfig() const { return api_config_; }
RegisterConfig ConfigManager::getRegisterConfig(uint8_t addr) const {
    for (const auto& config : register_configs_) {
        if (config.addr == addr) return config;
    }
    return RegisterConfig{};
}

AcquisitionConfig ConfigManager::getAcquisitionConfig() const { return acquisition_config_; }

LoggingConfig ConfigManager::getLoggingConfig() const { return logging_config_; }

// Validation methods
bool ConfigManager::validateSamplingInterval(uint32_t interval_ms, std::string& reason) const {
    if (interval_ms < validation_rules_.min_sampling_interval_ms) {
        reason = "Sampling interval too low (min: " + 
                 std::to_string(validation_rules_.min_sampling_interval_ms) + " ms)";
        return false;
    }
    if (interval_ms > validation_rules_.max_sampling_interval_ms) {
        reason = "Sampling interval too high (max: " + 
                 std::to_string(validation_rules_.max_sampling_interval_ms) + " ms)";
        return false;
    }
    return true;
}

bool ConfigManager::validateRegisters(const std::vector<uint8_t>& registers, std::string& reason) const {
    if (registers.empty() || registers.size() < validation_rules_.min_register_count) {
        reason = "Register count too low (min: " + 
                 std::to_string(validation_rules_.min_register_count) + ")";
        return false;
    }
    if (registers.size() > validation_rules_.max_register_count) {
        reason = "Register count too high (max: " + 
                 std::to_string(validation_rules_.max_register_count) + ")";
        return false;
    }
    
    // Check each register address is valid
    for (uint8_t reg : registers) {
        if (reg < validation_rules_.min_register_addr || reg > validation_rules_.max_register_addr) {
            reason = "Invalid register address: " + std::to_string(reg) + 
                     " (valid range: " + std::to_string(validation_rules_.min_register_addr) + 
                     "-" + std::to_string(validation_rules_.max_register_addr) + ")";
            return false;
        }
        
        // Check if register exists in our config
        bool found = false;
        for (const auto& rc : register_configs_) {
            if (rc.addr == reg) {
                found = true;
                break;
            }
        }
        if (!found) {
            reason = "Register " + std::to_string(reg) + " not defined in system";
            return false;
        }
    }
    
    // Check for duplicates
    for (size_t i = 0; i < registers.size(); i++) {
        for (size_t j = i + 1; j < registers.size(); j++) {
            if (registers[i] == registers[j]) {
                reason = "Duplicate register address: " + std::to_string(registers[i]);
                return false;
            }
        }
    }
    
    return true;
}

ConfigUpdateAck ConfigManager::applyConfigUpdate(const ConfigUpdateRequest& request) {
    ConfigUpdateAck ack;
    ack.nonce = request.nonce;
    ack.timestamp = millis();
    ack.all_success = true;
    
    Logger::info("[ConfigMgr] Processing config update request, nonce=%u", request.nonce);
    
    // Check if nonce already processed (idempotency)
    if (isNonceProcessed(request.nonce)) {
        Logger::warn("[ConfigMgr] Nonce %u already processed, ignoring duplicate request", request.nonce);
        ParameterAck nonce_ack;
        nonce_ack.parameter_name = "request";
        nonce_ack.result = ConfigUpdateResult::UNCHANGED;
        nonce_ack.reason = "Duplicate request (nonce already processed)";
        ack.unchanged.push_back(nonce_ack);
        ack.all_success = false;
        return ack;
    }
    
    // Process sampling interval update
    if (request.has_sampling_interval) {
        ParameterAck param_ack;
        param_ack.parameter_name = "sampling_interval";
        param_ack.old_value = std::to_string(acquisition_config_.polling_interval_ms);
        param_ack.new_value = std::to_string(request.sampling_interval_ms);
        
        // Check if value is unchanged
        if (request.sampling_interval_ms == acquisition_config_.polling_interval_ms) {
            param_ack.result = ConfigUpdateResult::UNCHANGED;
            param_ack.reason = "Value unchanged";
            ack.unchanged.push_back(param_ack);
            Logger::info("[ConfigMgr] Sampling interval unchanged: %u ms", request.sampling_interval_ms);
        } else {
            // Validate
            std::string reason;
            if (validateSamplingInterval(request.sampling_interval_ms, reason)) {
                // Apply
                acquisition_config_.polling_interval_ms = request.sampling_interval_ms;
                persistent_config_.sampling_interval_ms = request.sampling_interval_ms;
                param_ack.result = ConfigUpdateResult::ACCEPTED;
                param_ack.reason = "Applied successfully";
                ack.accepted.push_back(param_ack);
                Logger::info("[ConfigMgr] Sampling interval updated: %u -> %u ms", 
                             atoi(param_ack.old_value.c_str()), request.sampling_interval_ms);
            } else {
                // Reject
                param_ack.result = ConfigUpdateResult::REJECTED;
                param_ack.reason = reason;
                ack.rejected.push_back(param_ack);
                ack.all_success = false;
                Logger::warn("[ConfigMgr] Sampling interval rejected: %s", reason.c_str());
            }
        }
    }
    
    // Process register list update
    if (request.has_registers) {
        ParameterAck param_ack;
        param_ack.parameter_name = "registers";
        param_ack.old_value = getRegisterListString(acquisition_config_.minimum_registers);
        param_ack.new_value = getRegisterListString(request.registers);
        
        // Check if value is unchanged
        if (request.registers == acquisition_config_.minimum_registers) {
            param_ack.result = ConfigUpdateResult::UNCHANGED;
            param_ack.reason = "Value unchanged";
            ack.unchanged.push_back(param_ack);
            Logger::info("[ConfigMgr] Register list unchanged");
        } else {
            // Validate
            std::string reason;
            if (validateRegisters(request.registers, reason)) {
                // Apply
                acquisition_config_.minimum_registers = request.registers;
                persistent_config_.registers = request.registers;
                param_ack.result = ConfigUpdateResult::ACCEPTED;
                param_ack.reason = "Applied successfully";
                ack.accepted.push_back(param_ack);
                Logger::info("[ConfigMgr] Register list updated: [%s] -> [%s]", 
                             param_ack.old_value.c_str(), param_ack.new_value.c_str());
            } else {
                // Reject
                param_ack.result = ConfigUpdateResult::REJECTED;
                param_ack.reason = reason;
                ack.rejected.push_back(param_ack);
                ack.all_success = false;
                Logger::warn("[ConfigMgr] Register list rejected: %s", reason.c_str());
            }
        }
    }
    
    // If any parameters were accepted, save to persistent storage
    if (!ack.accepted.empty()) {
        persistent_config_.last_update_timestamp = ack.timestamp;
        if (savePersistentConfig()) {
            Logger::info("[ConfigMgr] Configuration persisted to flash");
        } else {
            Logger::error("[ConfigMgr] Failed to persist configuration!");
        }
    }
    
    // Mark nonce as processed
    markNonceProcessed(request.nonce);
    
    return ack;
}

// Persistence methods
bool ConfigManager::savePersistentConfig() {
    persistent_config_.checksum = calculateChecksum(persistent_config_);
    
    File file = LittleFS.open("/config/persistent.dat", "w");
    if (!file) {
        Logger::error("[ConfigMgr] Failed to open persistent config for writing");
        return false;
    }
    
    // Write binary data
    file.write((uint8_t*)&persistent_config_.version, sizeof(uint32_t));
    file.write((uint8_t*)&persistent_config_.sampling_interval_ms, sizeof(uint32_t));
    
    // Write register count and registers
    uint32_t reg_count = persistent_config_.registers.size();
    file.write((uint8_t*)&reg_count, sizeof(uint32_t));
    for (uint8_t reg : persistent_config_.registers) {
        file.write(&reg, sizeof(uint8_t));
    }
    
    file.write((uint8_t*)&persistent_config_.last_nonce, sizeof(uint32_t));
    file.write((uint8_t*)&persistent_config_.last_update_timestamp, sizeof(uint32_t));
    file.write((uint8_t*)&persistent_config_.checksum, sizeof(uint32_t));
    
    file.close();
    Logger::debug("[ConfigMgr] Persistent config saved");
    return true;
}

bool ConfigManager::loadPersistentConfig() {
    if (!LittleFS.exists("/config/persistent.dat")) {
        return false;
    }
    
    File file = LittleFS.open("/config/persistent.dat", "r");
    if (!file) {
        Logger::error("[ConfigMgr] Failed to open persistent config for reading");
        return false;
    }
    
    // Read binary data
    file.read((uint8_t*)&persistent_config_.version, sizeof(uint32_t));
    file.read((uint8_t*)&persistent_config_.sampling_interval_ms, sizeof(uint32_t));
    
    // Read register count and registers
    uint32_t reg_count = 0;
    file.read((uint8_t*)&reg_count, sizeof(uint32_t));
    persistent_config_.registers.clear();
    for (uint32_t i = 0; i < reg_count; i++) {
        uint8_t reg;
        file.read(&reg, sizeof(uint8_t));
        persistent_config_.registers.push_back(reg);
    }
    
    file.read((uint8_t*)&persistent_config_.last_nonce, sizeof(uint32_t));
    file.read((uint8_t*)&persistent_config_.last_update_timestamp, sizeof(uint32_t));
    file.read((uint8_t*)&persistent_config_.checksum, sizeof(uint32_t));
    
    file.close();
    
    // Verify checksum
    if (!verifyChecksum(persistent_config_)) {
        Logger::error("[ConfigMgr] Persistent config checksum mismatch!");
        return false;
    }
    
    Logger::debug("[ConfigMgr] Persistent config loaded successfully");
    return true;
}

// Runtime getters/setters
void ConfigManager::setSamplingInterval(uint32_t interval_ms) {
    acquisition_config_.polling_interval_ms = interval_ms;
}

uint32_t ConfigManager::getSamplingInterval() const {
    return acquisition_config_.polling_interval_ms;
}

void ConfigManager::setActiveRegisters(const std::vector<uint8_t>& registers) {
    acquisition_config_.minimum_registers = registers;
}

std::vector<uint8_t> ConfigManager::getActiveRegisters() const {
    return acquisition_config_.minimum_registers;
}

// Nonce management
bool ConfigManager::isNonceProcessed(uint32_t nonce) const {
    return nonce <= persistent_config_.last_nonce;
}

void ConfigManager::markNonceProcessed(uint32_t nonce) {
    if (nonce > persistent_config_.last_nonce) {
        persistent_config_.last_nonce = nonce;
    }
}

ConfigValidationRules ConfigManager::getValidationRules() const {
    return validation_rules_;
}

// Helper methods
uint32_t ConfigManager::calculateChecksum(const PersistentConfig& config) const {
    // Simple checksum: XOR all values
    uint32_t checksum = config.version ^ config.sampling_interval_ms ^ 
                        config.last_nonce ^ config.last_update_timestamp;
    for (uint8_t reg : config.registers) {
        checksum ^= reg;
    }
    return checksum;
}

bool ConfigManager::verifyChecksum(const PersistentConfig& config) const {
    uint32_t calculated = calculateChecksum(config);
    return calculated == config.checksum;
}

std::string ConfigManager::getRegisterListString(const std::vector<uint8_t>& registers) const {
    std::string result;
    for (size_t i = 0; i < registers.size(); i++) {
        result += std::to_string(registers[i]);
        if (i < registers.size() - 1) {
            result += ",";
        }
    }
    return result;
}

