#include "event_logger.hpp"
#include "logger.hpp"
#include <time.h>

// Initialize static members
bool EventLogger::begin(const char* logFilePath, uint16_t maxEvents) {
    log_file_path_ = logFilePath;
    max_events_ = maxEvents;
    event_count_ = 0;
    initialized_ = false;
    
    // Initialize LittleFS if not already done
    if (!LittleFS.begin()) {
        Logger::error("[EventLogger] Failed to mount LittleFS");
        return false;
    }
    
    // Load existing log
    if (!loadLog()) {
        Logger::warn("[EventLogger] No existing log found, creating new");
        // Create empty log file
        File file = LittleFS.open(log_file_path_.c_str(), "w");
        if (!file) {
            Logger::error("[EventLogger] Failed to create log file");
            return false;
        }
        file.print("[]");
        file.close();
        event_count_ = 0;
    }
    
    initialized_ = true;
    Logger::info("[EventLogger] Initialized with max %d events", max_events_);
    Logger::info("[EventLogger] Current event count: %d", event_count_);
    
    return true;
}

void EventLogger::logEvent(const String& event, 
                           EventModule module,
                           EventType type,
                           EventSeverity severity,
                           bool recovered,
                           const String& details) {
    if (!initialized_) {
        Logger::warn("[EventLogger] Not initialized, skipping log");
        return;
    }
    
    // Create event entry
    EventLogEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.event = event;
    entry.module = module;
    entry.type = type;
    entry.severity = severity;
    entry.recovered = recovered;
    entry.details = details;
    
    // Log to serial for real-time monitoring
    Logger::info("[EventLog] [%s] [%s] %s%s", 
                eventModuleToString(module),
                eventTypeToString(type),
                event.c_str(),
                recovered ? " (RECOVERED)" : "");
    
    if (!details.isEmpty()) {
        Logger::info("[EventLog]   Details: %s", details.c_str());
    }
    
    // Load existing log
    File file = LittleFS.open(log_file_path_.c_str(), "r");
    if (!file) {
        Logger::error("[EventLogger] Failed to open log file for reading");
        return;
    }
    
    // Parse existing JSON array
    DynamicJsonDocument doc(8192);  // 8KB for log entries
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error && error != DeserializationError::EmptyInput) {
        Logger::error("[EventLogger] Failed to parse log JSON: %s", error.c_str());
        // Start fresh if corrupt
        doc.clear();
        doc.to<JsonArray>();
    }
    
    // Get or create array
    JsonArray events = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc.to<JsonArray>();
    
    // Add new event
    JsonObject newEvent = events.createNestedObject();
    newEvent["timestamp"] = entry.timestamp;
    newEvent["event"] = entry.event;
    newEvent["module"] = eventModuleToString(entry.module);
    newEvent["type"] = eventTypeToString(entry.type);
    newEvent["severity"] = eventSeverityToString(entry.severity);
    newEvent["recovered"] = entry.recovered;
    if (!entry.details.isEmpty()) {
        newEvent["details"] = entry.details;
    }
    
    event_count_++;
    
    // Rotate if needed
    if (events.size() > max_events_) {
        events.remove(0);  // Remove oldest
    }
    
    // Save back to file
    file = LittleFS.open(log_file_path_.c_str(), "w");
    if (!file) {
        Logger::error("[EventLogger] Failed to open log file for writing");
        return;
    }
    
    serializeJson(doc, file);
    file.close();
}

void EventLogger::logFault(const String& event, EventModule module, bool recovered, const String& details) {
    logEvent(event, module, EventType::FAULT, EventSeverity::SEVERITY_HIGH, recovered, details);
}

void EventLogger::logRecovery(const String& event, EventModule module, const String& details) {
    logEvent(event, module, EventType::RECOVERY, EventSeverity::SEVERITY_MEDIUM, true, details);
}

void EventLogger::logError(const String& event, EventModule module, const String& details) {
    logEvent(event, module, EventType::ERROR, EventSeverity::SEVERITY_MEDIUM, false, details);
}

void EventLogger::logWarning(const String& event, EventModule module, const String& details) {
    logEvent(event, module, EventType::WARNING, EventSeverity::SEVERITY_LOW, false, details);
}

void EventLogger::logInfo(const String& event, EventModule module, const String& details) {
    logEvent(event, module, EventType::INFO, EventSeverity::SEVERITY_LOW, false, details);
}

String EventLogger::getEventsJson(uint16_t limitCount) {
    if (!initialized_) {
        return "[]";
    }
    
    File file = LittleFS.open(log_file_path_.c_str(), "r");
    if (!file) {
        return "[]";
    }
    
    String content = file.readString();
    file.close();
    
    if (limitCount == 0 || limitCount >= event_count_) {
        return content;
    }
    
    // Parse and limit
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, content);
    JsonArray events = doc.as<JsonArray>();
    
    // Create limited array
    DynamicJsonDocument limitedDoc(8192);
    JsonArray limitedEvents = limitedDoc.to<JsonArray>();
    
    int startIndex = max(0, (int)events.size() - (int)limitCount);
    for (int i = startIndex; i < events.size(); i++) {
        limitedEvents.add(events[i]);
    }
    
    String result;
    serializeJson(limitedDoc, result);
    return result;
}

String EventLogger::getEventsByModule(EventModule module, uint16_t limitCount) {
    File file = LittleFS.open(log_file_path_.c_str(), "r");
    if (!file) {
        return "[]";
    }
    
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, file);
    file.close();
    
    JsonArray events = doc.as<JsonArray>();
    
    DynamicJsonDocument filteredDoc(8192);
    JsonArray filteredEvents = filteredDoc.to<JsonArray>();
    
    const char* moduleStr = eventModuleToString(module);
    uint16_t count = 0;
    
    for (JsonObject event : events) {
        if (event["module"] == moduleStr) {
            filteredEvents.add(event);
            count++;
            if (limitCount > 0 && count >= limitCount) break;
        }
    }
    
    String result;
    serializeJson(filteredDoc, result);
    return result;
}

String EventLogger::getEventsByType(EventType type, uint16_t limitCount) {
    File file = LittleFS.open(log_file_path_.c_str(), "r");
    if (!file) {
        return "[]";
    }
    
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, file);
    file.close();
    
    JsonArray events = doc.as<JsonArray>();
    
    DynamicJsonDocument filteredDoc(8192);
    JsonArray filteredEvents = filteredDoc.to<JsonArray>();
    
    const char* typeStr = eventTypeToString(type);
    uint16_t count = 0;
    
    for (JsonObject event : events) {
        if (event["type"] == typeStr) {
            filteredEvents.add(event);
            count++;
            if (limitCount > 0 && count >= limitCount) break;
        }
    }
    
    String result;
    serializeJson(filteredDoc, result);
    return result;
}

uint16_t EventLogger::getFaultCount(EventModule module) {
    File file = LittleFS.open(log_file_path_.c_str(), "r");
    if (!file) {
        return 0;
    }
    
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, file);
    file.close();
    
    JsonArray events = doc.as<JsonArray>();
    
    const char* moduleStr = eventModuleToString(module);
    uint16_t count = 0;
    
    for (JsonObject event : events) {
        if (event["module"] == moduleStr && 
            (event["type"] == "FAULT" || event["type"] == "ERROR")) {
            count++;
        }
    }
    
    return count;
}

float EventLogger::getRecoveryRate(EventModule module) {
    File file = LittleFS.open(log_file_path_.c_str(), "r");
    if (!file) {
        return 0.0f;
    }
    
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, file);
    file.close();
    
    JsonArray events = doc.as<JsonArray>();
    
    const char* moduleStr = eventModuleToString(module);
    uint16_t faultCount = 0;
    uint16_t recoveredCount = 0;
    
    for (JsonObject event : events) {
        if (event["module"] == moduleStr && event["type"] == "FAULT") {
            faultCount++;
            if (event["recovered"] == true) {
                recoveredCount++;
            }
        }
    }
    
    if (faultCount == 0) return 100.0f;  // No faults = 100% recovery rate
    
    return (float)recoveredCount / (float)faultCount * 100.0f;
}

void EventLogger::clearLog() {
    File file = LittleFS.open(log_file_path_.c_str(), "w");
    if (file) {
        file.print("[]");
        file.close();
        event_count_ = 0;
        Logger::info("[EventLogger] Log cleared");
    }
}

void EventLogger::printLog() {
    String json = getEventsJson();
    Logger::info("[EventLogger] Complete Log:");
    Logger::info("%s", json.c_str());
}

void EventLogger::sync() {
    // LittleFS auto-syncs, but we can force it
    // This is a placeholder for future enhancement
}

bool EventLogger::loadLog() {
    File file = LittleFS.open(log_file_path_.c_str(), "r");
    if (!file) {
        return false;
    }
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Logger::error("[EventLogger] Failed to parse existing log: %s", error.c_str());
        return false;
    }
    
    if (doc.is<JsonArray>()) {
        JsonArray events = doc.as<JsonArray>();
        event_count_ = events.size();
        return true;
    }
    
    return false;
}

bool EventLogger::saveLog() {
    // Already handled in logEvent
    return true;
}

void EventLogger::rotateLog() {
    // Already handled in logEvent
}

String EventLogger::getCurrentTimestamp() {
    // Use millis() based timestamp since we don't have RTC
    // Format: "2025-09-04T17:25:00Z" (simplified with millis)
    unsigned long ms = millis();
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "2025-09-22T%02lu:%02lu:%02luZ",
             (hours % 24), (minutes % 60), (seconds % 60));
    
    return String(timestamp);
}

const char* EventLogger::eventTypeToString(EventType type) {
    switch (type) {
        case EventType::INFO: return "INFO";
        case EventType::WARNING: return "WARNING";
        case EventType::ERROR: return "ERROR";
        case EventType::FAULT: return "FAULT";
        case EventType::RECOVERY: return "RECOVERY";
        default: return "UNKNOWN";
    }
}

const char* EventLogger::eventModuleToString(EventModule module) {
    switch (module) {
        case EventModule::ACQUISITION: return "acquisition_task";
        case EventModule::INVERTER_SIM: return "inverter_sim";
        case EventModule::NETWORK: return "network";
        case EventModule::BUFFER: return "buffer";
        case EventModule::SECURITY: return "security";
        case EventModule::FOTA: return "fota";
        case EventModule::CONFIG: return "config";
        case EventModule::POWER: return "power";
        case EventModule::SYSTEM: return "system";
        default: return "unknown";
    }
}

const char* EventLogger::eventSeverityToString(EventSeverity severity) {
    switch (severity) {
        case EventSeverity::SEVERITY_LOW: return "LOW";
        case EventSeverity::SEVERITY_MEDIUM: return "MEDIUM";
        case EventSeverity::SEVERITY_HIGH: return "HIGH";
        case EventSeverity::SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}
