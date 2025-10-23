
#pragma once

#include <string>
#include <exception>

enum ErrorCode {
    ERR_NONE = 0,
    ERR_MODBUS_CRC,
    ERR_MODBUS_TIMEOUT,
    ERR_HTTP,
    ERR_CONFIG,
    ERR_UNKNOWN
};

class ModbusException : public std::exception {
public:
    ModbusException(const std::string& msg, ErrorCode code = ERR_MODBUS_CRC) : message_(msg), code_(code) {}
    const char* what() const noexcept override { return message_.c_str(); }
    ErrorCode code() const { return code_; }
    virtual ~ModbusException() noexcept {}
private:
    std::string message_;
    ErrorCode code_;
};

class HttpException : public std::exception {
public:
    HttpException(const std::string& msg, ErrorCode code = ERR_HTTP) : message_(msg), code_(code) {}
    const char* what() const noexcept override { return message_.c_str(); }
    ErrorCode code() const { return code_; }
    virtual ~HttpException() noexcept {}
private:
    std::string message_;
    ErrorCode code_;
};
