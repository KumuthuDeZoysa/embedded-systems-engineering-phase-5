#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <string>
#include "../include/ticker_fallback.hpp"
#include "../include/remote_config_handler.hpp"
#include "../include/logger.hpp"

RemoteConfigHandler* RemoteConfigHandler::instance_ = nullptr;

RemoteConfigHandler::RemoteConfigHandler(ConfigManager* config, SecureHttpClient* secure_http, CommandExecutor* cmd_executor)
    : pollTicker_(pollTaskWrapper, 60000), config_(config), secure_http_(secure_http), cmd_executor_(cmd_executor) {
    instance_ = this;
}
RemoteConfigHandler::~RemoteConfigHandler() { end(); }

void RemoteConfigHandler::begin(uint32_t interval_ms) {
    pollInterval_ = interval_ms;
    pollTicker_.interval(pollInterval_);
    running_ = true;
    pollTicker_.start();
}

void RemoteConfigHandler::end() {
    pollTicker_.stop();
    running_ = false;
}

void RemoteConfigHandler::loop() {
    if (running_) {
        pollTicker_.update();
    }
}

void RemoteConfigHandler::pollTask() {
    checkForConfigUpdate();
}

void RemoteConfigHandler::onConfigUpdate(std::function<void()> callback) {
    onUpdateCallback_ = callback;
}

void RemoteConfigHandler::onCommand(std::function<void(const CommandRequest&)> callback) {
    onCommandCallback_ = callback;
}

void RemoteConfigHandler::checkForConfigUpdate() {
    if (!secure_http_ || !config_) return;
    
    Logger::info("[RemoteCfg] Checking for config updates from cloud...");
    
    // Use SIMPLE endpoint without security for testing
    std::string simple_endpoint = "/api/inverter/config/simple";
    std::string plain_response;
    
    Logger::info("[RemoteCfg] Requesting endpoint: %s", simple_endpoint.c_str());
    Logger::info("[RemoteCfg] Using PLAIN HTTP GET (no security)");
    
    // Make plain HTTP GET request (no security)
    EcoHttpResponse resp = secure_http_->getHttpClient()->get(simple_endpoint.c_str());
    plain_response = resp.body;
    
    Logger::info("[RemoteCfg] Response status: %d", resp.status_code);
    Logger::info("[RemoteCfg] Response body length: %u bytes", (unsigned)plain_response.length());
    
    if (!resp.isSuccess()) {
        Logger::warn("[RemoteCfg] Failed to get config from cloud: status=%d", resp.status_code);
        return;
    }
    
    // Parse the response for config updates (use plain_response instead of resp.body)
    ConfigUpdateRequest request;
    if (parseConfigUpdateRequest(plain_response.c_str(), request)) {
        // Apply the configuration update
        ConfigUpdateAck ack = config_->applyConfigUpdate(request);
        
        // Send acknowledgment back to cloud
        sendConfigAck(ack);
        
        // Trigger callback if any parameters were accepted
        if (!ack.accepted.empty() && onUpdateCallback_) {
            onUpdateCallback_();
        }
    }
    
    // Parse the response for commands (use plain_response)
    CommandRequest command;
    if (parseCommandRequest(plain_response.c_str(), command)) {
        Logger::info("[RemoteCfg] Received command: id=%u, action=%s, target=%s, value=%.2f",
                     command.command_id, command.action.c_str(), 
                     command.target_register.c_str(), command.value);
        
        // Queue command for execution if executor is available
        if (cmd_executor_) {
            cmd_executor_->queueCommand(command);
        }
        
        // Trigger callback if set
        if (onCommandCallback_) {
            onCommandCallback_(command);
        }
    }
}

void RemoteConfigHandler::checkForCommands() {
    // Execute any pending commands
    if (cmd_executor_) {
        cmd_executor_->executePendingCommands();
        
        // Get executed results
        std::vector<CommandResult> results = cmd_executor_->getExecutedResults();
        
        // Send results back to cloud if any
        if (!results.empty()) {
            sendCommandResults(results);
            cmd_executor_->clearExecutedResults();
        }
    }
}

bool RemoteConfigHandler::parseConfigUpdateRequest(const char* json, ConfigUpdateRequest& request) {
    DynamicJsonDocument doc(1024);  // Allocate on heap instead of stack
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Logger::error("[RemoteCfg] JSON parse error: %s", error.c_str());
        return false;
    }
    
    // Initialize request
    request.has_sampling_interval = false;
    request.has_registers = false;
    request.nonce = 0;
    request.timestamp = millis();
    
    // Parse nonce (always present in secured responses)
    if (doc.containsKey("nonce")) {
        request.nonce = doc["nonce"].as<uint32_t>();
    } else {
        // Generate nonce from timestamp if not provided
        request.nonce = request.timestamp;
    }
    
    // Check for status field first
    if (doc.containsKey("status")) {
        String status = doc["status"].as<String>();
        if (status == "no_config") {
            Logger::debug("[RemoteCfg] Server reports no pending config");
            return false; // No config to process, but this is normal
        }
    }
    
    // Check if there's a config_update object
    if (!doc.containsKey("config_update")) {
        Logger::debug("[RemoteCfg] No config_update in response");
        return false;
    }
    
    JsonObject config_update = doc["config_update"];
    
    // Parse sampling interval
    if (config_update.containsKey("sampling_interval")) {
        // Convert from seconds to milliseconds (as per spec)
        uint32_t interval_seconds = config_update["sampling_interval"].as<uint32_t>();
        request.sampling_interval_ms = interval_seconds * 1000;
        request.has_sampling_interval = true;
        Logger::debug("[RemoteCfg] Parsed sampling_interval: %u ms", request.sampling_interval_ms);
    }
    
    // Parse registers
    if (config_update.containsKey("registers")) {
        JsonArray registers = config_update["registers"];
        request.registers.clear();
        
        for (JsonVariant reg : registers) {
            // Support both numeric and string register names
            if (reg.is<int>()) {
                request.registers.push_back(reg.as<uint8_t>());
            } else if (reg.is<const char*>()) {
                // Map register name to address
                std::string name = reg.as<const char*>();
                // Simple mapping (you can expand this)
                if (name == "voltage") request.registers.push_back(0);
                else if (name == "current") request.registers.push_back(1);
                else if (name == "frequency") request.registers.push_back(2);
                else if (name == "pv1_voltage") request.registers.push_back(3);
                else if (name == "pv2_voltage") request.registers.push_back(4);
                else if (name == "pv1_current") request.registers.push_back(5);
                else if (name == "pv2_current") request.registers.push_back(6);
                else if (name == "temperature") request.registers.push_back(7);
                else if (name == "export_power") request.registers.push_back(8);
                else if (name == "output_power") request.registers.push_back(9);
                else {
                    Logger::warn("[RemoteCfg] Unknown register name: %s", name.c_str());
                }
            }
        }
        
        if (!request.registers.empty()) {
            request.has_registers = true;
            Logger::debug("[RemoteCfg] Parsed %u registers", (unsigned)request.registers.size());
        }
    }
    
    return request.has_sampling_interval || request.has_registers;
}

void RemoteConfigHandler::sendConfigAck(const ConfigUpdateAck& ack) {
    std::string ackJson = generateAckJson(ack);
    
    Logger::info("[RemoteCfg] Sending config acknowledgment to cloud");
    Logger::debug("[RemoteCfg] Ack JSON: %s", ackJson.c_str());
    
    // Send ACK to cloud using secured POST
    std::string ack_endpoint = config_->getApiConfig().config_endpoint + "/ack";
    std::string plain_response;
    EcoHttpResponse resp = secure_http_->securePost(ack_endpoint.c_str(), ackJson, plain_response);
    
    if (resp.isSuccess()) {
        Logger::info("[RemoteCfg] Config acknowledgment sent successfully");
    } else {
        Logger::warn("[RemoteCfg] Failed to send config acknowledgment: status=%d", resp.status_code);
    }
}

std::string RemoteConfigHandler::generateAckJson(const ConfigUpdateAck& ack) {
    DynamicJsonDocument doc(2048);  // Allocate on heap instead of stack
    
    doc["nonce"] = ack.nonce;
    doc["timestamp"] = ack.timestamp;
    doc["all_success"] = ack.all_success;
    
    // Create config_ack object
    JsonObject config_ack = doc.createNestedObject("config_ack");
    
    // Accepted parameters
    JsonArray accepted = config_ack.createNestedArray("accepted");
    for (const auto& param : ack.accepted) {
        JsonObject p = accepted.createNestedObject();
        p["parameter"] = param.parameter_name;
        p["old_value"] = param.old_value;
        p["new_value"] = param.new_value;
        p["reason"] = param.reason;
    }
    
    // Rejected parameters
    JsonArray rejected = config_ack.createNestedArray("rejected");
    for (const auto& param : ack.rejected) {
        JsonObject p = rejected.createNestedObject();
        p["parameter"] = param.parameter_name;
        p["old_value"] = param.old_value;
        p["new_value"] = param.new_value;
        p["reason"] = param.reason;
    }
    
    // Unchanged parameters
    JsonArray unchanged = config_ack.createNestedArray("unchanged");
    for (const auto& param : ack.unchanged) {
        JsonObject p = unchanged.createNestedObject();
        p["parameter"] = param.parameter_name;
        p["reason"] = param.reason;
    }
    
    // Serialize to string
    std::string result;
    serializeJson(doc, result);
    return result;
}

void RemoteConfigHandler::pollTaskWrapper() {
    if (instance_) {
        instance_->pollTask();
    }
}

bool RemoteConfigHandler::parseCommandRequest(const char* json, CommandRequest& command) {
    DynamicJsonDocument doc(1024);  // Allocate on heap instead of stack
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Logger::error("[RemoteCfg] JSON parse error for command: %s", error.c_str());
        return false;
    }
    
    // Check if there's a command object
    if (!doc.containsKey("command")) {
        Logger::debug("[RemoteCfg] No command in response");
        return false;
    }
    
    JsonObject cmd = doc["command"];
    
    // Parse command fields
    if (!cmd.containsKey("command_id") || !cmd.containsKey("action") || 
        !cmd.containsKey("target_register") || !cmd.containsKey("value")) {
        Logger::warn("[RemoteCfg] Command missing required fields");
        return false;
    }
    
    command.command_id = cmd["command_id"].as<uint32_t>();
    command.action = cmd["action"].as<const char*>();
    command.target_register = cmd["target_register"].as<const char*>();
    command.value = cmd["value"].as<float>();
    command.timestamp = cmd.containsKey("timestamp") ? cmd["timestamp"].as<uint32_t>() : millis();
    command.nonce = cmd.containsKey("nonce") ? cmd["nonce"].as<uint32_t>() : command.timestamp;
    
    Logger::debug("[RemoteCfg] Parsed command: id=%u, action=%s, target=%s, value=%.2f",
                  command.command_id, command.action.c_str(), 
                  command.target_register.c_str(), command.value);
    
    return true;
}

void RemoteConfigHandler::sendCommandResults(const std::vector<CommandResult>& results) {
    if (results.empty()) {
        return;
    }
    
    std::string resultsJson = generateCommandResultsJson(results);
    
    Logger::info("[RemoteCfg] Sending %u command results to cloud", (unsigned)results.size());
    Logger::debug("[RemoteCfg] Results JSON: %s", resultsJson.c_str());
    
    // Send results to cloud using secured POST
    std::string result_endpoint = config_->getApiConfig().config_endpoint + "/command/result";
    std::string plain_response;
    EcoHttpResponse resp = secure_http_->securePost(result_endpoint.c_str(), resultsJson, plain_response);
    
    if (resp.isSuccess()) {
        Logger::info("[RemoteCfg] Command results sent successfully");
    } else {
        Logger::warn("[RemoteCfg] Failed to send command results: status=%d", resp.status_code);
    }
}

std::string RemoteConfigHandler::generateCommandResultsJson(const std::vector<CommandResult>& results) {
    DynamicJsonDocument doc(2048);  // Allocate on heap instead of stack
    
    doc["timestamp"] = millis();
    doc["result_count"] = results.size();
    
    JsonArray results_array = doc.createNestedArray("command_results");
    
    for (const auto& result : results) {
        JsonObject r = results_array.createNestedObject();
        r["command_id"] = result.command_id;
        r["status"] = commandStatusToString(result.status);
        r["status_message"] = result.status_message;
        r["executed_at"] = result.executed_at;
        
        if (result.status == CommandStatus::SUCCESS) {
            r["actual_value"] = result.actual_value;
        }
        
        if (!result.error_details.empty()) {
            r["error_details"] = result.error_details;
        }
    }
    
    std::string output;
    serializeJson(doc, output);
    return output;
}
