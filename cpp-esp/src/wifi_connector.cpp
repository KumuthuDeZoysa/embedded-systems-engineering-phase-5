#include "../include/wifi_connector.hpp"
#include "../include/logger.hpp"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#else
#warning "WiFiConnector: Unknown platform; WiFi connect will be a no-op"
#endif

#include <Arduino.h>


WiFiConnector::WiFiConnector() {
    // Hardcoded credentials
    ssid_ = "PCD";
    password_ = "12345678";
}
WiFiConnector::~WiFiConnector() {}

void WiFiConnector::begin() {
    if (ssid_.empty()) return;
    Logger::info("WiFiConnector: Connecting to SSID: %s", ssid_.c_str());
#if defined(ESP8266) || defined(ESP32)
    WiFi.mode(WIFI_STA);
#if defined(ESP32)
    WiFi.setAutoReconnect(true);
#endif
    WiFi.begin(ssid_.c_str(), password_.c_str());
    lastAttemptMs_ = millis();
#else
    (void)ssid_;
    (void)password_;
#endif
}

void WiFiConnector::loop() {
    if (ssid_.empty()) return;
#if defined(ESP8266) || defined(ESP32)
    if (WiFi.status() == WL_CONNECTED) return;
    unsigned long now = millis();
    if (now - lastAttemptMs_ >= reconnectIntervalMs_) {
        Logger::info("WiFiConnector: Attempting reconnect to %s", ssid_.c_str());
        WiFi.disconnect();
        WiFi.begin(ssid_.c_str(), password_.c_str());
        lastAttemptMs_ = now;
    }
#endif
}

bool WiFiConnector::isConnected() const {
#if defined(ESP8266) || defined(ESP32)
    return WiFi.status() == WL_CONNECTED;
#else
    return false;
#endif
}
