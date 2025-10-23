#include <Arduino.h>
#if defined(ESP32) || defined(ESP8266)
#include <HTTPClient.h>
#endif
#include "../include/http_client.hpp"

EcoHttpClient::EcoHttpClient(const std::string& base_url, uint32_t timeout_ms)
    : base_url_(base_url), timeout_ms_(timeout_ms) {}

EcoHttpClient::~EcoHttpClient() {}

void EcoHttpClient::setDefaultHeaders(const char* keys[], const char* values[], int count) {
    default_header_count = (count > 10) ? 10 : count;
    for (int i = 0; i < default_header_count; ++i) {
        default_header_keys[i] = keys[i];
        default_header_values[i] = values[i];
    }
}

EcoHttpResponse EcoHttpClient::post(const char* endpoint, const char* data, size_t len,
                              const char* content_type,
                              const char* header_keys[], const char* header_values[], int header_count) {
    EcoHttpResponse response;
    char url[256];
    // If endpoint starts with "http", treat it as a full URL
    if (strncmp(endpoint, "http", 4) == 0) {
        snprintf(url, sizeof(url), "%s", endpoint);
    } else {
        snprintf(url, sizeof(url), "%s%s", base_url_.c_str(), endpoint);
    }
    HTTPClient http;
    #if defined(ESP8266)
        WiFiClient client;
        http.begin(client, url);
        uint32_t to_sec = timeout_ms_ ? ((timeout_ms_ + 999) / 1000) : 5;
        http.setTimeout((int)to_sec);
    #elif defined(ESP32)
        http.begin(url);
    #endif
    if (content_type && strlen(content_type) > 0) {
        http.addHeader("Content-Type", content_type);
    } else {
        http.addHeader("Content-Type", "application/json");
    }
    // Add default headers
    for (int i = 0; i < default_header_count; ++i) {
        http.addHeader(default_header_keys[i].c_str(), default_header_values[i].c_str());
        response.header_keys[i] = default_header_keys[i];
        response.header_values[i] = default_header_values[i];
    }
    // Add custom headers
    int total_headers = default_header_count;
    for (int i = 0; i < header_count && total_headers < 10; ++i, ++total_headers) {
        http.addHeader(header_keys[i], header_values[i]);
        response.header_keys[total_headers] = header_keys[i];
        response.header_values[total_headers] = header_values[i];
    }
    response.header_count = total_headers;
    int httpCode = http.POST((uint8_t*)data, len);
    response.status_code = httpCode;
    {
        String s = http.getString();
        response.body = std::string(s.c_str());
    }
    http.end();
    return response;
}

EcoHttpResponse EcoHttpClient::get(const char* endpoint,
                             const char* header_keys[], const char* header_values[], int header_count) {
    EcoHttpResponse response;
    char url[256];
    // If endpoint starts with "http", treat it as a full URL
    if (strncmp(endpoint, "http", 4) == 0) {
        snprintf(url, sizeof(url), "%s", endpoint);
    } else {
        snprintf(url, sizeof(url), "%s%s", base_url_.c_str(), endpoint);
    }
    HTTPClient http;
    #if defined(ESP8266)
        WiFiClient client;
        http.begin(client, url);
        uint32_t to_sec = timeout_ms_ ? ((timeout_ms_ + 999) / 1000) : 5;
        http.setTimeout((int)to_sec);
    #elif defined(ESP32)
        http.begin(url);
    #endif
    // Add default headers
    for (int i = 0; i < default_header_count; ++i) {
        http.addHeader(default_header_keys[i].c_str(), default_header_values[i].c_str());
        response.header_keys[i] = default_header_keys[i];
        response.header_values[i] = default_header_values[i];
    }
    // Add custom headers
    int total_headers = default_header_count;
    for (int i = 0; i < header_count && total_headers < 10; ++i, ++total_headers) {
        http.addHeader(header_keys[i], header_values[i]);
        response.header_keys[total_headers] = header_keys[i];
        response.header_values[total_headers] = header_values[i];
    }
    response.header_count = total_headers;
    int httpCode = http.GET();
    response.status_code = httpCode;
    {
        String s = http.getString();
        response.body = std::string(s.c_str());
    }
    http.end();
    return response;
}
