#include "../include/secure_http_client.hpp"
#include "../include/logger.hpp"
#include <Arduino.h>

SecureHttpClient::SecureHttpClient(const std::string& base_url,
                                 SecurityLayer* security,
                                 uint32_t timeout_ms)
    : security_(security)
    , security_enabled_(true)
    , owns_http_client_(true) {
    
    http_client_ = new EcoHttpClient(base_url, timeout_ms);
}

// Constructor that uses existing HTTP client
SecureHttpClient::SecureHttpClient(EcoHttpClient* http_client,
                                 SecurityLayer* security)
    : http_client_(http_client)
    , security_(security)
    , security_enabled_(true)
    , owns_http_client_(false) {
    // Don't own the HTTP client, so don't delete it in destructor
}

SecureHttpClient::~SecureHttpClient() {
    if (owns_http_client_ && http_client_) {
        delete http_client_;
        http_client_ = nullptr;
    }
}

EcoHttpResponse SecureHttpClient::securePost(const char* endpoint,
                                            const std::string& payload,
                                            std::string& plain_response) {
    EcoHttpResponse response;
    
    if (!http_client_) {
        Logger::error("[SecureHttp] HTTP client not initialized");
        response.status_code = 0;
        return response;
    }
    
    // If security disabled, do plain HTTP
    if (!security_enabled_ || !security_) {
        Logger::debug("[SecureHttp] Security disabled, using plain HTTP POST");
        response = http_client_->post(endpoint, payload.c_str(), payload.length());
        plain_response = response.body;
        return response;
    }
    
    // Logger::debug("[SecureHttp] Securing POST payload: %u bytes", (unsigned)payload.length());
    
    // Secure the outgoing payload
    SecuredMessage secured_msg;
    SecurityResult sec_result = security_->secureMessage(payload, secured_msg);
    
    if (!sec_result.is_success()) {
        Logger::error("[SecureHttp] Failed to secure message: %s", 
                     sec_result.error_message.c_str());
        response.status_code = 0;
        return response;
    }
    
    // Generate secured envelope JSON
    std::string secured_json = security_->generateSecuredEnvelope(secured_msg);
    
    // Simple nonce display (minimal stack usage)
    Logger::info("[SecureHttp] Upload nonce: %u", secured_msg.nonce);
    
    // Send secured payload
    response = http_client_->post(endpoint, secured_json.c_str(), 
                                 secured_json.length(), "application/json");
    
    if (!response.isSuccess()) {
        Logger::warn("[SecureHttp] HTTP POST failed: status=%d", response.status_code);
        return response;
    }
    
    // Verify and extract response
    sec_result = security_->verifyMessage(response.body, plain_response);
    
    if (!sec_result.is_success()) {
        Logger::error("[SecureHttp] Failed to verify response: %s",
                     sec_result.error_message.c_str());
        
        // Don't fail the HTTP request, but log the security issue
        // Some endpoints may return plain responses
        Logger::warn("[SecureHttp] Treating response as plain text");
        plain_response = response.body;
    }
    
    // Logger::debug("[SecureHttp] POST completed: status=%d, response=%u bytes",
    //              response.status_code, (unsigned)plain_response.length());
    
    return response;
}

EcoHttpResponse SecureHttpClient::secureGet(const char* endpoint,
                                           std::string& plain_response) {
    EcoHttpResponse response;
    
    if (!http_client_) {
        Logger::error("[SecureHttp] HTTP client not initialized");
        response.status_code = 0;
        return response;
    }
    
    // If security disabled, do plain HTTP
    if (!security_enabled_ || !security_) {
        Logger::debug("[SecureHttp] Security disabled, using plain HTTP GET");
        response = http_client_->get(endpoint);
        plain_response = response.body;
        return response;
    }
    
    Logger::debug("[SecureHttp] Performing secure GET request");
    
    // For GET requests, we can add security headers instead of body encryption
    // Add nonce as query parameter or header
    uint32_t nonce = security_->getNextNonce();
    
    // Add custom headers with nonce and Unix timestamp (seconds since 1970)
    const char* header_keys[] = {"X-Nonce", "X-Timestamp"};
    char nonce_str[32];
    char timestamp_str[32];
    snprintf(nonce_str, sizeof(nonce_str), "%u", nonce);
    // Get Unix timestamp from NTP/RTC - for now use approximation
    // In production, implement proper time sync via NTP
    unsigned long unix_timestamp = 1704067200 + (millis() / 1000); // ~2024-01-01 base
    snprintf(timestamp_str, sizeof(timestamp_str), "%lu", unix_timestamp);
    const char* header_values[] = {nonce_str, timestamp_str};
    
    // Compute HMAC over: endpoint + nonce + timestamp
    // Pre-allocate to avoid stack pressure from string concatenation
    std::string hmac_input;
    hmac_input.reserve(strlen(endpoint) + strlen(nonce_str) + strlen(timestamp_str));
    hmac_input.append(endpoint);
    hmac_input.append(nonce_str);
    hmac_input.append(timestamp_str);
    std::string mac = security_->computeHMAC(hmac_input, security_->getConfig().psk);
    
    // Add MAC header
    const char* all_keys[] = {"X-Nonce", "X-Timestamp", "X-MAC"};
    const char* all_values[] = {nonce_str, timestamp_str, mac.c_str()};
    
    Logger::debug("[SecureHttp] GET with nonce=%u, mac=%s...", 
                 nonce, mac.substr(0, 8).c_str());
    
    // Send GET request with security headers
    response = http_client_->get(endpoint, all_keys, all_values, 3);
    
    if (!response.isSuccess()) {
        Logger::warn("[SecureHttp] HTTP GET failed: status=%d", response.status_code);
        return response;
    }
    
    // Try to verify and extract response if it's secured
    SecurityResult sec_result = security_->verifyMessage(response.body, plain_response);
    
    if (!sec_result.is_success()) {
        // Response might not be secured, treat as plain
        Logger::debug("[SecureHttp] Response not secured, treating as plain text");
        plain_response = response.body;
    }
    
    Logger::debug("[SecureHttp] GET completed: status=%d, response=%u bytes",
                 response.status_code, (unsigned)plain_response.length());
    
    return response;
}
