#include "../include/command_executor.hpp"
#include "../include/logger.hpp"
#include <Arduino.h>
#include <cstring>

CommandExecutor::CommandExecutor(ProtocolAdapter* adapter, ConfigManager* config, EcoHttpClient* http)
    : adapter_(adapter), config_(config), http_(http) {
}

CommandExecutor::~CommandExecutor() {
}

bool CommandExecutor::queueCommand(const CommandRequest& command) {
    // Check if already processed (idempotency)
    if (isCommandProcessed(command.command_id)) {
        Logger::warn("[CmdExec] Command %u already processed, ignoring duplicate", command.command_id);
        return false;
    }
    
    // Check queue size
    if (command_queue_.size() >= MAX_QUEUE_SIZE) {
        Logger::error("[CmdExec] Command queue full (%u commands), cannot queue new command", 
                      (unsigned)command_queue_.size());
        return false;
    }
    
    // Validate command
    std::string error_reason;
    if (!validateCommand(command, error_reason)) {
        Logger::error("[CmdExec] Command validation failed: %s", error_reason.c_str());
        
        // Create failed result immediately
        CommandResult result;
        result.command_id = command.command_id;
        result.status = CommandStatus::INVALID_REGISTER;
        result.status_message = "Validation failed";
        result.error_details = error_reason;
        result.executed_at = millis();
        result.actual_value = 0.0f;
        
        executed_results_.push_back(result);
        processed_command_ids_.push_back(command.command_id);
        
        return false;
    }
    
    // Queue the command
    QueuedCommand queued;
    queued.request = command;
    queued.executed = false;
    queued.queued_at = millis();
    queued.retry_count = 0;
    
    command_queue_.push_back(queued);
    Logger::info("[CmdExec] Queued command %u: action=%s, target=%s, value=%.2f", 
                 command.command_id, command.action.c_str(), 
                 command.target_register.c_str(), command.value);
    
    return true;
}

void CommandExecutor::executePendingCommands() {
    if (command_queue_.empty()) {
        return;
    }
    
    Logger::info("[CmdExec] Executing %u pending commands", (unsigned)command_queue_.size());
    
    std::vector<QueuedCommand> remaining;
    
    for (auto& queued : command_queue_) {
        if (queued.executed) {
            continue; // Already executed, skip
        }
        
        // Execute the command
        CommandResult result = executeCommand(queued.request);
        
        // Store result
        queued.result = result;
        queued.executed = true;
        
        // Add to executed results
        executed_results_.push_back(result);
        
        // Mark as processed
        processed_command_ids_.push_back(queued.request.command_id);
        
        // Maintain processed IDs size
        if (processed_command_ids_.size() > MAX_PROCESSED_IDS) {
            processed_command_ids_.erase(processed_command_ids_.begin());
        }
        
        // Trigger callback if set
        if (onExecutedCallback_) {
            onExecutedCallback_(result);
        }
        
        Logger::info("[CmdExec] Command %u executed: status=%s", 
                     result.command_id, commandStatusToString(result.status));
    }
    
    // Clear executed commands from queue
    command_queue_.clear();
    
    // Maintain results size
    while (executed_results_.size() > MAX_RESULTS_SIZE) {
        executed_results_.erase(executed_results_.begin());
    }
}

std::vector<CommandResult> CommandExecutor::getExecutedResults() {
    return executed_results_;
}

void CommandExecutor::clearExecutedResults() {
    executed_results_.clear();
    Logger::debug("[CmdExec] Cleared executed results");
}

bool CommandExecutor::isCommandProcessed(uint32_t command_id) const {
    for (uint32_t id : processed_command_ids_) {
        if (id == command_id) {
            return true;
        }
    }
    return false;
}

void CommandExecutor::onCommandExecuted(std::function<void(const CommandResult&)> callback) {
    onExecutedCallback_ = callback;
}

CommandResult CommandExecutor::executeCommand(const CommandRequest& command) {
    CommandResult result;
    result.command_id = command.command_id;
    result.executed_at = millis();
    result.actual_value = 0.0f;
    
    Logger::info("[CmdExec] Executing command %u: %s on %s with value %.2f", 
                 command.command_id, command.action.c_str(), 
                 command.target_register.c_str(), command.value);
    
    // Check if action is write_register
    if (command.action != "write_register") {
        result.status = CommandStatus::FAILED;
        result.status_message = "Unsupported action";
        result.error_details = "Only 'write_register' action is supported";
        Logger::error("[CmdExec] Unsupported action: %s", command.action.c_str());
        return result;
    }
    
    // Resolve register address
    uint8_t reg_addr;
    if (!resolveRegisterAddress(command.target_register, reg_addr)) {
        result.status = CommandStatus::INVALID_REGISTER;
        result.status_message = "Invalid register";
        result.error_details = "Register '" + command.target_register + "' not found or not writable";
        Logger::error("[CmdExec] Register resolution failed: %s", command.target_register.c_str());
        return result;
    }
    
    // Get register configuration
    RegisterConfig reg_config = config_->getRegisterConfig(reg_addr);
    
    // Check if register is writable
    if (reg_config.access.find("Write") == std::string::npos) {
        result.status = CommandStatus::INVALID_REGISTER;
        result.status_message = "Register not writable";
        result.error_details = "Register " + std::to_string(reg_addr) + " is read-only";
        Logger::error("[CmdExec] Register %u is not writable", reg_addr);
        return result;
    }
    
    // Convert value to raw value using gain
    uint16_t raw_value;
    if (reg_config.gain > 0) {
        raw_value = (uint16_t)(command.value * reg_config.gain);
    } else {
        raw_value = (uint16_t)(command.value);
    }
    
    Logger::debug("[CmdExec] Writing register %u: value=%.2f, raw_value=%u, gain=%.2f", 
                  reg_addr, command.value, raw_value, reg_config.gain);
    
    // Execute write via protocol adapter
    ModbusConfig mb_config = config_->getModbusConfig();
    int max_retries = mb_config.max_retries > 0 ? mb_config.max_retries : 3;
    bool success = false;
    
    for (int attempt = 0; attempt < max_retries && !success; attempt++) {
        if (adapter_->writeRegister(reg_addr, raw_value)) {
            success = true;
            break;
        }
        
        if (attempt < max_retries - 1) {
            Logger::warn("[CmdExec] Write attempt %d failed, retrying...", attempt + 1);
            if (mb_config.retry_delay_ms > 0) {
                delay(mb_config.retry_delay_ms);
            }
        }
    }
    
    if (success) {
        result.status = CommandStatus::SUCCESS;
        result.status_message = "Command executed successfully";
        result.actual_value = command.value;
        result.error_details = "";
        Logger::info("[CmdExec] Command %u executed successfully: reg=%u, value=%.2f", 
                     command.command_id, reg_addr, command.value);
    } else {
        result.status = CommandStatus::TIMEOUT;
        result.status_message = "Write operation failed";
        result.error_details = "Failed to write to register " + std::to_string(reg_addr) + 
                               " after " + std::to_string(max_retries) + " attempts";
        Logger::error("[CmdExec] Command %u failed after %d attempts", 
                      command.command_id, max_retries);
    }
    
    return result;
}

bool CommandExecutor::validateCommand(const CommandRequest& command, std::string& error_reason) {
    // Validate action
    if (command.action != "write_register") {
        error_reason = "Unsupported action: " + command.action;
        return false;
    }
    
    // Validate target register is not empty
    if (command.target_register.empty()) {
        error_reason = "Target register cannot be empty";
        return false;
    }
    
    // Try to resolve register address
    uint8_t reg_addr;
    if (!resolveRegisterAddress(command.target_register, reg_addr)) {
        error_reason = "Unknown register: " + command.target_register;
        return false;
    }
    
    // Check if register is writable
    RegisterConfig reg_config = config_->getRegisterConfig(reg_addr);
    if (reg_config.access.find("Write") == std::string::npos) {
        error_reason = "Register is read-only: " + command.target_register;
        return false;
    }
    
    // Additional value validation could be added here
    // For example, check if value is within acceptable range
    
    return true;
}

bool CommandExecutor::resolveRegisterAddress(const std::string& register_name, uint8_t& address) {
    // Try to parse as numeric address first
    char* endptr;
    long addr_num = strtol(register_name.c_str(), &endptr, 10);
    if (*endptr == '\0' && addr_num >= 0 && addr_num <= 255) {
        // It's a valid numeric address
        address = (uint8_t)addr_num;
        return true;
    }
    
    // Map register names to addresses
    if (register_name == "voltage" || register_name == "Vac1_L1_Phase_voltage") {
        address = 0;
    } else if (register_name == "current" || register_name == "Iac1_L1_Phase_current") {
        address = 1;
    } else if (register_name == "frequency" || register_name == "Fac1_L1_Phase_frequency") {
        address = 2;
    } else if (register_name == "pv1_voltage" || register_name == "Vpv1_PV1_input_voltage") {
        address = 3;
    } else if (register_name == "pv2_voltage" || register_name == "Vpv2_PV2_input_voltage") {
        address = 4;
    } else if (register_name == "pv1_current" || register_name == "Ipv1_PV1_input_current") {
        address = 5;
    } else if (register_name == "pv2_current" || register_name == "Ipv2_PV2_input_current") {
        address = 6;
    } else if (register_name == "temperature" || register_name == "Inverter_internal_temperature") {
        address = 7;
    } else if (register_name == "export_power" || register_name == "Export_power_percentage") {
        address = 8;
    } else if (register_name == "output_power" || register_name == "Pac_L_Inverter_output_power") {
        address = 9;
    } else if (register_name == "status_flag") {
        // Special case for status_flag mentioned in spec - map to export_power
        address = 8;
    } else {
        // Unknown register name
        return false;
    }
    
    return true;
}
