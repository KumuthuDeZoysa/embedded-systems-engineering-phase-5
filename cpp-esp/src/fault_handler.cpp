#include "fault_handler.hpp"
#include "logger.hpp"

bool FaultHandler::begin(EventLogger* eventLogger) {
    if (!eventLogger) {
        Logger::error("[FaultHandler] Event logger is null");
        return false;
    }
    
    event_logger_ = eventLogger;
    initialized_ = true;
    degraded_mode_ = false;
    total_faults_ = 0;
    recovered_faults_ = 0;
    
    // Reset fault counters
    for (int i = 0; i < 8; i++) {
        fault_counts_[i] = 0;
    }
    
    Logger::info("[FaultHandler] Initialized");
    return true;
}

RecoveryResult FaultHandler::handleFault(FaultType type,
                                        EventModule module,
                                        const String& description,
                                        const String& details) {
    if (!initialized_) {
        Logger::error("[FaultHandler] Not initialized");
        return RecoveryResult::FAILED;
    }
    
    // Create fault context
    FaultContext context;
    context.type = type;
    context.module = module;
    context.description = description;
    context.details = details;
    context.retry_count = 0;
    context.timestamp_ms = millis();
    
    // Increment counters
    total_faults_++;
    if ((int)type < 8) {
        fault_counts_[(int)type]++;
    }
    
    Logger::warn("[FaultHandler] Handling fault: %s - %s", 
                faultTypeToString(type), description.c_str());
    
    // Attempt recovery
    RecoveryResult result = attemptRecovery(context);
    
    // Log the fault
    logFault(context, result == RecoveryResult::SUCCESS);
    
    if (result == RecoveryResult::SUCCESS) {
        recovered_faults_++;
    } else if (result == RecoveryResult::PARTIAL) {
        degraded_mode_ = true;
        Logger::warn("[FaultHandler] System entering degraded mode");
    }
    
    return result;
}

RecoveryResult FaultHandler::handleInverterTimeout(const String& details) {
    Logger::warn("[FaultHandler] Inverter SIM timeout detected");
    
    RecoveryResult result = handleFault(
        FaultType::INVERTER_TIMEOUT,
        EventModule::INVERTER_SIM,
        "Inverter SIM timeout",
        details.isEmpty() ? "No response from Inverter SIM within timeout period" : details
    );
    
    // Additional recovery actions
    if (result != RecoveryResult::SUCCESS) {
        Logger::warn("[FaultHandler] Timeout recovery failed, will retry on next cycle");
        // System will continue in degraded mode
        // Next acquisition cycle will retry
    }
    
    return result;
}

RecoveryResult FaultHandler::handleMalformedFrame(const String& frameData, const String& details) {
    Logger::warn("[FaultHandler] Malformed frame detected");
    
    String detailsStr = details;
    if (detailsStr.isEmpty()) {
        detailsStr = "CRC mismatch or corrupt data in frame";
    }
    if (!frameData.isEmpty()) {
        detailsStr += " | Frame: " + frameData;
    }
    
    RecoveryResult result = handleFault(
        FaultType::MALFORMED_FRAME,
        EventModule::INVERTER_SIM,
        "Malformed frame received",
        detailsStr
    );
    
    // Recovery: Discard frame and request retransmission
    Logger::info("[FaultHandler] Discarding malformed frame, will request fresh data");
    
    return result;
}

RecoveryResult FaultHandler::handleBufferOverflow(const String& details) {
    Logger::error("[FaultHandler] Buffer overflow detected");
    
    RecoveryResult result = handleFault(
        FaultType::BUFFER_OVERFLOW,
        EventModule::BUFFER,
        "Buffer overflow",
        details.isEmpty() ? "Local buffer capacity exceeded" : details
    );
    
    // Recovery: Force immediate upload or discard oldest data
    Logger::warn("[FaultHandler] Buffer overflow - may need emergency upload or data drop");
    
    // Set system to degraded mode temporarily
    degraded_mode_ = true;
    
    return RecoveryResult::PARTIAL;  // Partial recovery (data may be lost)
}

RecoveryResult FaultHandler::handleNetworkError(int httpCode, const String& details) {
    Logger::warn("[FaultHandler] Network error: HTTP %d", httpCode);
    
    String detailsStr = details;
    if (detailsStr.isEmpty()) {
        detailsStr = "HTTP error code: " + String(httpCode);
    }
    
    RecoveryResult result = handleFault(
        FaultType::NETWORK_ERROR,
        EventModule::NETWORK,
        "Network communication error",
        detailsStr
    );
    
    // Recovery strategies based on error code
    if (httpCode == -1) {
        Logger::warn("[FaultHandler] Connection failed, will retry with backoff");
        return RecoveryResult::RETRY_NEEDED;
    } else if (httpCode >= 500) {
        Logger::warn("[FaultHandler] Server error, will retry later");
        return RecoveryResult::RETRY_NEEDED;
    } else if (httpCode >= 400) {
        Logger::error("[FaultHandler] Client error, check request format");
        return RecoveryResult::FAILED;
    }
    
    return result;
}

RecoveryResult FaultHandler::handleParseError(const String& details) {
    Logger::warn("[FaultHandler] Parse error detected");
    
    RecoveryResult result = handleFault(
        FaultType::PARSE_ERROR,
        EventModule::SYSTEM,
        "Data parsing failed",
        details.isEmpty() ? "Failed to parse JSON or data format" : details
    );
    
    // Recovery: Skip malformed data and continue
    Logger::info("[FaultHandler] Skipping malformed data, continuing operation");
    
    return result;
}

RecoveryResult FaultHandler::handleSecurityViolation(const String& details) {
    Logger::error("[FaultHandler] SECURITY VIOLATION detected!");
    
    RecoveryResult result = handleFault(
        FaultType::SECURITY_VIOLATION,
        EventModule::SECURITY,
        "Security violation",
        details.isEmpty() ? "Auth failure or integrity check failed" : details
    );
    
    // Security violations are serious - no automatic recovery
    Logger::error("[FaultHandler] Security violation - rejecting request");
    
    return RecoveryResult::FAILED;
}

RecoveryResult FaultHandler::handleMemoryError(const String& details) {
    Logger::error("[FaultHandler] Memory error detected");
    
    RecoveryResult result = handleFault(
        FaultType::MEMORY_ERROR,
        EventModule::SYSTEM,
        "Memory allocation failed",
        details.isEmpty() ? "Out of memory or allocation failed" : details
    );
    
    // Recovery: Free up memory if possible
    Logger::warn("[FaultHandler] Attempting to free memory");
    
    // Set degraded mode
    degraded_mode_ = true;
    
    return RecoveryResult::PARTIAL;
}

void FaultHandler::getFaultStats(uint16_t& totalFaults, uint16_t& recoveredFaults, float& recoveryRate) {
    totalFaults = total_faults_;
    recoveredFaults = recovered_faults_;
    
    if (total_faults_ == 0) {
        recoveryRate = 100.0f;
    } else {
        recoveryRate = ((float)recovered_faults_ / (float)total_faults_) * 100.0f;
    }
}

uint16_t FaultHandler::getFaultCount(FaultType type) {
    if ((int)type < 8) {
        return fault_counts_[(int)type];
    }
    return 0;
}

void FaultHandler::resetCounters() {
    total_faults_ = 0;
    recovered_faults_ = 0;
    for (int i = 0; i < 8; i++) {
        fault_counts_[i] = 0;
    }
    degraded_mode_ = false;
    Logger::info("[FaultHandler] Counters reset");
}

unsigned long FaultHandler::getBackoffDelay(uint16_t retryCount) {
    unsigned long delay = RETRY_DELAY_MS;
    for (uint16_t i = 0; i < retryCount && i < 5; i++) {
        delay *= BACKOFF_MULTIPLIER;
    }
    return delay;
}

void FaultHandler::logFault(const FaultContext& context, bool recovered) {
    if (!event_logger_) return;
    
    event_logger_->logFault(
        context.description,
        context.module,
        recovered,
        context.details
    );
}

RecoveryResult FaultHandler::attemptRecovery(FaultContext& context) {
    // Generic recovery logic with exponential backoff
    
    switch (context.type) {
        case FaultType::INVERTER_TIMEOUT:
        case FaultType::NETWORK_ERROR:
            // Retry with backoff
            Logger::info("[FaultHandler] Will retry operation with backoff");
            return RecoveryResult::RETRY_NEEDED;
            
        case FaultType::MALFORMED_FRAME:
        case FaultType::PARSE_ERROR:
            // Discard and continue
            Logger::info("[FaultHandler] Discarding bad data, continuing");
            return RecoveryResult::SUCCESS;
            
        case FaultType::BUFFER_OVERFLOW:
        case FaultType::MEMORY_ERROR:
            // Partial recovery (degraded mode)
            Logger::warn("[FaultHandler] Entering degraded mode");
            return RecoveryResult::PARTIAL;
            
        case FaultType::SECURITY_VIOLATION:
            // No recovery for security issues
            Logger::error("[FaultHandler] Security fault - no recovery");
            return RecoveryResult::FAILED;
            
        default:
            Logger::warn("[FaultHandler] Unknown fault type, attempting generic recovery");
            return RecoveryResult::RETRY_NEEDED;
    }
}

const char* FaultHandler::faultTypeToString(FaultType type) {
    switch (type) {
        case FaultType::INVERTER_TIMEOUT: return "INVERTER_TIMEOUT";
        case FaultType::MALFORMED_FRAME: return "MALFORMED_FRAME";
        case FaultType::BUFFER_OVERFLOW: return "BUFFER_OVERFLOW";
        case FaultType::NETWORK_ERROR: return "NETWORK_ERROR";
        case FaultType::PARSE_ERROR: return "PARSE_ERROR";
        case FaultType::SECURITY_VIOLATION: return "SECURITY_VIOLATION";
        case FaultType::MEMORY_ERROR: return "MEMORY_ERROR";
        case FaultType::UNKNOWN: return "UNKNOWN";
        default: return "UNDEFINED";
    }
}
