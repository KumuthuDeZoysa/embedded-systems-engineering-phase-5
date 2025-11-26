#ifndef EVENT_LOGGER_HPP
#define EVENT_LOGGER_HPP

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

/**
 * @brief Event types for fault logging
 */
enum class EventType {
    INFO,
    WARNING,
    ERROR,
    FAULT,
    RECOVERY
};

/**
 * @brief Event categories/modules
 */
enum class EventModule {
    ACQUISITION,
    INVERTER_SIM,
    NETWORK,
    BUFFER,
    SECURITY,
    FOTA,
    CONFIG,
    POWER,
    SYSTEM
};

/**
 * @brief Event severity levels
 */
enum class EventSeverity {
    SEVERITY_LOW,
    SEVERITY_MEDIUM,
    SEVERITY_HIGH,
    SEVERITY_CRITICAL
};

/**
 * @brief Event log entry structure
 */
struct EventLogEntry {
    String timestamp;           // ISO 8601 format: "2025-09-04T17:25:00Z"
    String event;              // Event description
    EventModule module;        // Which module/subsystem
    EventType type;            // Event type
    EventSeverity severity;    // Severity level
    bool recovered;            // Whether fault was recovered
    String details;            // Additional details (optional)
};

/**
 * @brief Persistent Event Logger for fault tracking and recovery
 * 
 * Stores events to LittleFS in JSON format for post-mortem analysis
 * and real-time fault monitoring.
 */
class EventLogger {
public:
    /**
     * @brief Initialize the event logger
     * @param logFilePath Path to log file in LittleFS
     * @param maxEvents Maximum events to keep (rotation)
     * @return true if initialized successfully
     */
    bool begin(const char* logFilePath = "/event_log.json", uint16_t maxEvents = 100);
    
    /**
     * @brief Log an event
     * @param event Event description
     * @param module Module/subsystem generating the event
     * @param type Event type
     * @param severity Event severity
     * @param recovered Whether fault was recovered
     * @param details Additional details (optional)
     */
    void logEvent(const String& event, 
                  EventModule module,
                  EventType type = EventType::INFO,
                  EventSeverity severity = EventSeverity::SEVERITY_LOW,
                  bool recovered = false,
                  const String& details = "");
    
    /**
     * @brief Log a fault event (shorthand)
     */
    void logFault(const String& event, EventModule module, bool recovered = false, const String& details = "");
    
    /**
     * @brief Log a recovery event (shorthand)
     */
    void logRecovery(const String& event, EventModule module, const String& details = "");
    
    /**
     * @brief Log an error event (shorthand)
     */
    void logError(const String& event, EventModule module, const String& details = "");
    
    /**
     * @brief Log a warning event (shorthand)
     */
    void logWarning(const String& event, EventModule module, const String& details = "");
    
    /**
     * @brief Log an info event (shorthand)
     */
    void logInfo(const String& event, EventModule module, const String& details = "");
    
    /**
     * @brief Get all logged events as JSON array
     * @param limitCount Maximum number of recent events to return (0 = all)
     * @return JSON array string
     */
    String getEventsJson(uint16_t limitCount = 0);
    
    /**
     * @brief Get events filtered by module
     */
    String getEventsByModule(EventModule module, uint16_t limitCount = 0);
    
    /**
     * @brief Get events filtered by type
     */
    String getEventsByType(EventType type, uint16_t limitCount = 0);
    
    /**
     * @brief Get fault count for a specific module
     */
    uint16_t getFaultCount(EventModule module);
    
    /**
     * @brief Get recovery rate for a specific module
     */
    float getRecoveryRate(EventModule module);
    
    /**
     * @brief Clear all logged events
     */
    void clearLog();
    
    /**
     * @brief Get total event count
     */
    uint16_t getEventCount() const { return event_count_; }
    
    /**
     * @brief Export log to serial (for debugging)
     */
    void printLog();
    
    /**
     * @brief Sync log to flash (call periodically)
     */
    void sync();

private:
    String log_file_path_;
    uint16_t max_events_;
    uint16_t event_count_;
    bool initialized_;
    
    /**
     * @brief Load existing log from file
     */
    bool loadLog();
    
    /**
     * @brief Save log to file
     */
    bool saveLog();
    
    /**
     * @brief Rotate log if needed (keep last N events)
     */
    void rotateLog();
    
    /**
     * @brief Get current timestamp (ISO 8601)
     */
    String getCurrentTimestamp();
    
    /**
     * @brief Convert enum to string
     */
    static const char* eventTypeToString(EventType type);
    static const char* eventModuleToString(EventModule module);
    static const char* eventSeverityToString(EventSeverity severity);
};

#endif // EVENT_LOGGER_HPP
