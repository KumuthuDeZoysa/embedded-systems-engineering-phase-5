#pragma once
#include <stdint.h>
#include <vector>
#include <string>

// Configuration update request from cloud
struct ConfigUpdateRequest {
    uint32_t sampling_interval_ms;  // New sampling interval (0 = no change)
    std::vector<uint8_t> registers; // New register list (empty = no change)
    bool has_sampling_interval;     // Flag indicating if sampling interval is in request
    bool has_registers;             // Flag indicating if registers are in request
    uint32_t nonce;                 // For idempotency and deduplication
    uint32_t timestamp;             // Request timestamp
};

// Result of applying a single parameter
enum class ConfigUpdateResult {
    ACCEPTED,       // Parameter was valid and applied
    REJECTED,       // Parameter was invalid or unsafe
    UNCHANGED,      // Parameter is same as current (idempotent)
    ERROR           // Error during application
};

// Acknowledgment for a specific parameter
struct ParameterAck {
    std::string parameter_name;
    ConfigUpdateResult result;
    std::string reason;  // Human-readable reason for rejection/error
    std::string old_value;
    std::string new_value;
};

// Full acknowledgment message sent back to cloud
struct ConfigUpdateAck {
    uint32_t nonce;                          // Match request nonce
    uint32_t timestamp;                      // When update was applied
    std::vector<ParameterAck> accepted;
    std::vector<ParameterAck> rejected;
    std::vector<ParameterAck> unchanged;
    bool all_success;                        // True if all parameters accepted
};

// Validation constraints
struct ConfigValidationRules {
    // Sampling interval constraints
    uint32_t min_sampling_interval_ms = 1000;    // 1 second minimum
    uint32_t max_sampling_interval_ms = 300000;  // 5 minutes maximum
    
    // Register constraints
    uint8_t min_register_addr = 0;
    uint8_t max_register_addr = 9;
    size_t min_register_count = 1;
    size_t max_register_count = 10;
    
    // General constraints
    uint32_t max_nonce_age_ms = 300000;  // 5 minutes max age for deduplication
};

// Persistent configuration state (saved to flash)
struct PersistentConfig {
    uint32_t version;                    // Config version for migration
    uint32_t sampling_interval_ms;
    std::vector<uint8_t> registers;
    uint32_t last_nonce;                 // Last processed nonce
    uint32_t last_update_timestamp;      // When last updated
    uint32_t checksum;                   // Simple integrity check
};
