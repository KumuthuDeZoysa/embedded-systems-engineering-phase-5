#pragma once

#include "http_client.hpp"
#include "security_layer.hpp"
#include <string>

/**
 * @file secure_http_client.hpp
 * @brief Secure HTTP client wrapper with integrated security layer
 * 
 * Wraps EcoHttpClient to automatically apply security to all
 * outgoing requests and verify incoming responses.
 * 
 * Milestone 4 Part 3: Security Implementation
 */

class SecureHttpClient {
public:
    /**
     * @brief Constructor (creates new HTTP client)
     * @param base_url Base URL for HTTP requests
     * @param security Security layer instance
     * @param timeout_ms Request timeout in milliseconds
     */
    SecureHttpClient(const std::string& base_url, 
                    SecurityLayer* security,
                    uint32_t timeout_ms = 5000);
    
    /**
     * @brief Constructor (uses existing HTTP client)
     * @param http_client Existing HTTP client instance
     * @param security Security layer instance
     */
    SecureHttpClient(EcoHttpClient* http_client, 
                    SecurityLayer* security);
    
    ~SecureHttpClient();
    
    /**
     * @brief Perform secure POST request
     * @param endpoint API endpoint
     * @param payload Plain JSON payload
     * @param secured_response Output secured response
     * @return HTTP response
     */
    EcoHttpResponse securePost(const char* endpoint, 
                               const std::string& payload,
                               std::string& plain_response);
    
    /**
     * @brief Perform secure GET request
     * @param endpoint API endpoint
     * @param plain_response Output plain response
     * @return HTTP response
     */
    EcoHttpResponse secureGet(const char* endpoint,
                              std::string& plain_response);
    
    /**
     * @brief Get underlying HTTP client
     * @return Pointer to EcoHttpClient
     */
    EcoHttpClient* getHttpClient() { return http_client_; }
    
    /**
     * @brief Get security layer
     * @return Pointer to SecurityLayer
     */
    SecurityLayer* getSecurityLayer() { return security_; }
    
    /**
     * @brief Enable/disable security
     * @param enabled True to enable security
     */
    void setSecurityEnabled(bool enabled) { security_enabled_ = enabled; }
    
    /**
     * @brief Check if security is enabled
     * @return True if security is enabled
     */
    bool isSecurityEnabled() const { return security_enabled_; }
    
private:
    EcoHttpClient* http_client_;
    SecurityLayer* security_;
    bool security_enabled_;
    bool owns_http_client_;
};
