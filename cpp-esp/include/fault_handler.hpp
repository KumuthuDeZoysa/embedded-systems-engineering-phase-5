#ifndef FAULT_HANDLER_HPP
#define FAULT_HANDLER_HPP

#include <Arduino.h>
#include "event_logger.hpp"

/**
 * @brief Fault types that can occur
 */
enum class FaultType {
    INVERTER_TIMEOUT,      // Inverter SIM not responding
    MALFORMED_FRAME,       // CRC error or corrupt data
    BUFFER_OVERFLOW,       // Local buffer full
    NETWORK_ERROR,         // HTTP/WiFi failure
    PARSE_ERROR,           // JSON/data parsing failed
    SECURITY_VIOLATION,    // Auth/integrity check failed
    MEMORY_ERROR,          // Out of memory
    UNKNOWN                // Uncategorized fault
};

/**
 * @brief Recovery action result
 */
enum class RecoveryResult {
    SUCCESS,               // Fault recovered successfully
    PARTIAL,               // Partial recovery (degraded mode)
    FAILED,                // Recovery attempt failed
    RETRY_NEEDED           // Need to retry recovery
};

/**
 * @brief Fault context information
 */
struct FaultContext {
    FaultType type;
    EventModule module;
    String description;
    String details;
    uint16_t retry_count;
    unsigned long timestamp_ms;
};

/**
 * @brief Fault Handler - Manages fault detection, recovery, and logging
 * 
 * Handles various fault scenarios and implements recovery strategies:
 * - Inverter SIM timeouts
 * - Malformed frames (CRC errors, corrupt data)
 * - Buffer overflow
 * - Network errors
 * - etc.
 */
class FaultHandler {
public:
    /**
     * @brief Initialize fault handler
     * @param eventLogger Pointer to event logger instance
     */
    bool begin(EventLogger* eventLogger);
    
    /**
     * @brief Report a fault and attempt recovery
     * @param type Fault type
     * @param module Module where fault occurred
     * @param description Fault description
     * @param details Additional details
     * @return Recovery result
     */
    RecoveryResult handleFault(FaultType type,
                              EventModule module,
                              const String& description,
                              const String& details = "");
    
    /**
     * @brief Handle Inverter SIM timeout
     */
    RecoveryResult handleInverterTimeout(const String& details = "");
    
    /**
     * @brief Handle malformed frame (CRC error, corrupt data)
     */
    RecoveryResult handleMalformedFrame(const String& frameData, const String& details = "");
    
    /**
     * @brief Handle buffer overflow
     */
    RecoveryResult handleBufferOverflow(const String& details = "");
    
    /**
     * @brief Handle network error
     */
    RecoveryResult handleNetworkError(int httpCode, const String& details = "");
    
    /**
     * @brief Handle parse error (JSON, data format)
     */
    RecoveryResult handleParseError(const String& details = "");
    
    /**
     * @brief Handle security violation
     */
    RecoveryResult handleSecurityViolation(const String& details = "");
    
    /**
     * @brief Handle memory error
     */
    RecoveryResult handleMemoryError(const String& details = "");
    
    /**
     * @brief Get fault statistics
     */
    void getFaultStats(uint16_t& totalFaults, uint16_t& recoveredFaults, float& recoveryRate);
    
    /**
     * @brief Get fault count for specific type
     */
    uint16_t getFaultCount(FaultType type);
    
    /**
     * @brief Check if system is in degraded mode
     */
    bool isDegradedMode() const { return degraded_mode_; }
    
    /**
     * @brief Clear degraded mode flag
     */
    void clearDegradedMode() { degraded_mode_ = false; }
    
    /**
     * @brief Reset fault counters
     */
    void resetCounters();

private:
    EventLogger* event_logger_;
    bool initialized_;
    bool degraded_mode_;
    
    // Fault counters
    uint16_t total_faults_;
    uint16_t recovered_faults_;
    uint16_t fault_counts_[8];  // Per fault type
    
    // Retry configuration
    static const uint16_t MAX_RETRIES = 3;
    static const unsigned long RETRY_DELAY_MS = 1000;
    static const unsigned long BACKOFF_MULTIPLIER = 2;
    
    /**
     * @brief Get exponential backoff delay
     */
    unsigned long getBackoffDelay(uint16_t retryCount);
    
    /**
     * @brief Log fault with event logger
     */
    void logFault(const FaultContext& context, bool recovered);
    
    /**
     * @brief Attempt generic recovery
     */
    RecoveryResult attemptRecovery(FaultContext& context);
    
    /**
     * @brief Convert fault type to string
     */
    static const char* faultTypeToString(FaultType type);
};

#endif // FAULT_HANDLER_HPP
