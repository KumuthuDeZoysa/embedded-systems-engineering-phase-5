#pragma once
#include <string>
#include <cstdint>

class WiFiConnector {
public:
    WiFiConnector();
    ~WiFiConnector();

    // Start connection using hardcoded credentials
    void begin();
    // Non-blocking loop; will attempt reconnects periodically
    void loop();
    bool isConnected() const;

private:
    std::string ssid_;
    std::string password_;
    uint32_t lastAttemptMs_ = 0;
    uint32_t reconnectIntervalMs_ = 10000; // try every 10s
};
