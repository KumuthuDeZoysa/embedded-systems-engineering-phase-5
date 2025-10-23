#pragma once
#include <stdint.h>
#include <string>

struct EcoHttpResponse {
    int status_code = 0;
    std::string body;
    // Simple key-value pairs for headers
    std::string header_keys[10];
    std::string header_values[10];
    int header_count = 0;
    bool isSuccess() const { return status_code >= 200 && status_code < 300; }
};

class EcoHttpClient {
public:
    EcoHttpClient(const std::string& base_url, uint32_t timeout_ms = 5000);
    ~EcoHttpClient();

    EcoHttpResponse post(const char* endpoint, const char* data, size_t len,
                     const char* content_type = nullptr,
                     const char* header_keys[] = nullptr, const char* header_values[] = nullptr, int header_count = 0);
    EcoHttpResponse get(const char* endpoint,
                    const char* header_keys[] = nullptr, const char* header_values[] = nullptr, int header_count = 0);

    void setDefaultHeaders(const char* keys[], const char* values[], int count);

private:
    std::string base_url_;
    uint32_t timeout_ms_;
    std::string default_header_keys[10];
    std::string default_header_values[10];
    int default_header_count = 0;
};
