#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include "../include/logger.hpp"
#include <stdarg.h>
#include <string.h>
#include <cstdio>


Logger::Level Logger::min_level_ = Logger::INFO;
std::string Logger::log_file_ = "/logs/main.log";
bool Logger::flush_on_write_ = true;

void Logger::begin(const LoggingConfig& cfg) {
    // Try to mount LittleFS; auto-format if mount fails (safer for fresh flash)
    if (!LittleFS.begin(true)) return;
    min_level_ = Logger::INFO;
    if (!cfg.log_level.empty()) {
        if (strcmp(cfg.log_level.c_str(), "DEBUG") == 0) min_level_ = Logger::DEBUG;
        else if (strcmp(cfg.log_level.c_str(), "INFO") == 0) min_level_ = Logger::INFO;
        else if (strcmp(cfg.log_level.c_str(), "WARN") == 0) min_level_ = Logger::WARN;
        else if (strcmp(cfg.log_level.c_str(), "ERROR") == 0) min_level_ = Logger::ERROR;
    }
    log_file_ = !cfg.log_file.empty() ? cfg.log_file : "/logs/main.log";
    flush_on_write_ = cfg.flush_on_write;
}

void Logger::write_log(Level level, const char* fmt, va_list args) {
    if (level < min_level_) return;
    char buf[128]; // Reduce buffer size for stack safety
    (void)vsnprintf(buf, sizeof(buf), fmt, args);
    const char* level_str = "INFO";
    switch (level) {
        case Logger::DEBUG: level_str = "DEBUG"; break;
        case Logger::INFO:  level_str = "INFO"; break;
        case Logger::WARN:  level_str = "WARN"; break;
        case Logger::ERROR: level_str = "ERROR"; break;
    }
    // Add timestamp like cpp: [YYYY-MM-DD HH:MM:SS] [LEVEL] message
    // For ESP32, use a simple timestamp since no RTC by default
    unsigned long ms = millis();
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    char time_buf[32];
    snprintf(time_buf, sizeof(time_buf), "%04lu-%02lu-%02lu %02lu:%02lu:%02lu.%03lu",
             2025UL, 9UL, 22UL + days, hours % 24, minutes % 60, seconds % 60, ms % 1000);
    
    // Print to serial only (disable LittleFS logging to avoid boot issues)
    printf("[%s] [%s] %s\n", time_buf, level_str, buf);
    
    /* DISABLED: LittleFS logging causing boot crashes
    File fh = LittleFS.open(log_file_.c_str(), "a+");
    if (fh) {
        fh.printf("[%s] [%s] %s\n", time_buf, level_str, buf);
        if (flush_on_write_) fh.flush();
        fh.close();
    }
    */
}

void Logger::log(Level level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log(level, fmt, args);
    va_end(args);
}

void Logger::debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log(DEBUG, fmt, args);
    va_end(args);
}

void Logger::info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log(INFO, fmt, args);
    va_end(args);
}

void Logger::warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log(WARN, fmt, args);
    va_end(args);
}

void Logger::error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log(ERROR, fmt, args);
    va_end(args);
}

void Logger::flush() {
    File fh = LittleFS.open(log_file_.c_str(), "a"); // Open in append mode to flush
    if (fh) {
        fh.flush();
        fh.close();
    }
}

void Logger::shutdown() {
    // No persistent file handle to close
}
