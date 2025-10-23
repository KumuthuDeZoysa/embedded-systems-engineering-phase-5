#pragma once
#include "command_execution.hpp"
#include "protocol_adapter.hpp"
#include "config_manager.hpp"
#include "http_client.hpp"
#include <vector>
#include <functional>

class CommandExecutor {
public:
    CommandExecutor(ProtocolAdapter* adapter, ConfigManager* config, EcoHttpClient* http);
    ~CommandExecutor();
    
    // Queue a command for execution
    bool queueCommand(const CommandRequest& command);
    
    // Execute all pending commands
    void executePendingCommands();
    
    // Get results of executed commands
    std::vector<CommandResult> getExecutedResults();
    
    // Clear executed results after reporting
    void clearExecutedResults();
    
    // Check if command with ID already processed (idempotency)
    bool isCommandProcessed(uint32_t command_id) const;
    
    // Callback when command is executed
    void onCommandExecuted(std::function<void(const CommandResult&)> callback);
    
private:
    ProtocolAdapter* adapter_;
    ConfigManager* config_;
    EcoHttpClient* http_;
    
    std::vector<QueuedCommand> command_queue_;
    std::vector<CommandResult> executed_results_;
    std::vector<uint32_t> processed_command_ids_;
    
    std::function<void(const CommandResult&)> onExecutedCallback_;
    
    // Execute a single command
    CommandResult executeCommand(const CommandRequest& command);
    
    // Validate command before execution
    bool validateCommand(const CommandRequest& command, std::string& error_reason);
    
    // Convert register name to address
    bool resolveRegisterAddress(const std::string& register_name, uint8_t& address);
    
    const size_t MAX_QUEUE_SIZE = 10;
    const size_t MAX_RESULTS_SIZE = 20;
    const size_t MAX_PROCESSED_IDS = 50;
};
