
#pragma once

#include <string>
#include <cstdarg>
#include <Arduino.h>
#include "config_manager.hpp"

class Logger {
public:
    enum Level { DEBUG, INFO, WARN, ERROR };
    static void begin(const LoggingConfig& cfg);
    static void log(Level level, const char* fmt, ...);
    static void debug(const char* fmt, ...);
    static void info(const char* fmt, ...);
    static void warn(const char* fmt, ...);
    static void error(const char* fmt, ...);
    static void flush();
    static void shutdown();
private:
    static Level min_level_;
    static std::string log_file_;
    static bool flush_on_write_;
    static void write_log(Level level, const char* fmt, va_list args);
};
