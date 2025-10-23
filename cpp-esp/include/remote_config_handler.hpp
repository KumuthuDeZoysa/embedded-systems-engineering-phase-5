#pragma once
#include "config_manager.hpp"
#include "http_client.hpp"
#include "secure_http_client.hpp"
#include "config_update.hpp"
#include "command_executor.hpp"

class SecureHttpClient;
class CommandExecutor;
#include "ticker_fallback.hpp"
#include <cstdint>
#include <functional>

class RemoteConfigHandler {
public:
    RemoteConfigHandler(ConfigManager* config, SecureHttpClient* secure_http, CommandExecutor* cmd_executor = nullptr);
    ~RemoteConfigHandler();

    void begin(uint32_t interval_ms = 60000);
    void end();
    void loop();
    void checkForConfigUpdate();
    void sendConfigAck(const ConfigUpdateAck& ack);
    void onConfigUpdate(std::function<void()> callback);
    void onCommand(std::function<void(const CommandRequest&)> callback);
    
    // Command execution methods
    void checkForCommands();
    void sendCommandResults(const std::vector<CommandResult>& results);
    
    // Parse config update request from JSON
    bool parseConfigUpdateRequest(const char* json, ConfigUpdateRequest& request);
    
    // Parse command request from JSON
    bool parseCommandRequest(const char* json, CommandRequest& command);
    
    // Generate acknowledgment JSON
    std::string generateAckJson(const ConfigUpdateAck& ack);
    
    // Generate command results JSON
    std::string generateCommandResultsJson(const std::vector<CommandResult>& results);

private:
    Ticker pollTicker_;
    uint32_t pollInterval_ = 60000;
    bool running_ = false;
    ConfigManager* config_ = nullptr;
    SecureHttpClient* secure_http_ = nullptr;
    CommandExecutor* cmd_executor_ = nullptr;
    std::function<void(const CommandRequest&)> onCommandCallback_ = nullptr;
    std::function<void()> onUpdateCallback_ = nullptr;
    void pollTask();
    static void pollTaskWrapper();
    static RemoteConfigHandler* instance_;
};
