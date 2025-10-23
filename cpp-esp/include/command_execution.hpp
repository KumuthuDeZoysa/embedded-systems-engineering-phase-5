#pragma once
#include <stdint.h>
#include <string>
#include <cstring>

// Command request from cloud to device
struct CommandRequest {
    uint32_t command_id;              // Unique command identifier
    std::string action;               // Action type (e.g., "write_register")
    std::string target_register;      // Target register name or address
    float value;                      // Value to write
    uint32_t timestamp;               // When command was issued
    uint32_t nonce;                   // For security and deduplication
};

// Command execution result from device to cloud
enum class CommandStatus {
    SUCCESS,                          // Command executed successfully
    FAILED,                           // Command execution failed
    INVALID_REGISTER,                 // Register doesn't exist or not writable
    INVALID_VALUE,                    // Value out of range
    TIMEOUT,                          // Communication timeout with Inverter SIM
    PENDING,                          // Command queued but not executed yet
    UNKNOWN                           // Unknown error
};

struct CommandResult {
    uint32_t command_id;              // Matches command_id from request
    CommandStatus status;             // Execution status
    std::string status_message;       // Human-readable status message
    uint32_t executed_at;             // Timestamp when executed
    float actual_value;               // Value that was written (if successful)
    std::string error_details;        // Additional error information
};

// Command queue entry (internal device use)
struct QueuedCommand {
    CommandRequest request;
    bool executed;
    CommandResult result;
    uint32_t queued_at;               // When command was received
    uint8_t retry_count;              // Number of execution attempts
};

// Helper to convert status to string
inline const char* commandStatusToString(CommandStatus status) {
    switch (status) {
        case CommandStatus::SUCCESS: return "success";
        case CommandStatus::FAILED: return "failed";
        case CommandStatus::INVALID_REGISTER: return "invalid_register";
        case CommandStatus::INVALID_VALUE: return "invalid_value";
        case CommandStatus::TIMEOUT: return "timeout";
        case CommandStatus::PENDING: return "pending";
        case CommandStatus::UNKNOWN: return "unknown";
        default: return "unknown";
    }
}

// Helper to convert string to status
inline CommandStatus stringToCommandStatus(const char* str) {
    if (strcmp(str, "success") == 0) return CommandStatus::SUCCESS;
    if (strcmp(str, "failed") == 0) return CommandStatus::FAILED;
    if (strcmp(str, "invalid_register") == 0) return CommandStatus::INVALID_REGISTER;
    if (strcmp(str, "invalid_value") == 0) return CommandStatus::INVALID_VALUE;
    if (strcmp(str, "timeout") == 0) return CommandStatus::TIMEOUT;
    if (strcmp(str, "pending") == 0) return CommandStatus::PENDING;
    return CommandStatus::UNKNOWN;
}
