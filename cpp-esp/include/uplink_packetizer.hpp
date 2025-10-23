
#pragma once
#include "data_storage.hpp"
#include "http_client.hpp"
#include "secure_http_client.hpp"

class EcoHttpClient;
class SecureHttpClient;

#include "ticker_fallback.hpp"

class UplinkPacketizer {
public:
    UplinkPacketizer(DataStorage* storage, SecureHttpClient* secure_http);
    ~UplinkPacketizer();

    void begin(uint32_t interval_ms = 60000);
    void end();
         void setCloudEndpoint(const std::string& url);
    void loop();

private:
    bool drainOutbox_();
    Ticker uploadTicker_;
    uint32_t uploadInterval_ = 60000;
         std::string cloudUrl_;
    bool running_ = false;
    DataStorage* storage_ = nullptr;
    SecureHttpClient* secure_http_ = nullptr;
    void uploadTask();
    static void uploadTaskWrapper();
    static UplinkPacketizer* instance_;
    void compressData(const Sample* samples, size_t sampleCount, char* outBuf, size_t outBufSize, size_t& outLen);
    void encryptData(const char* inBuf, size_t inLen, char* outBuf, size_t outBufSize, size_t& outLen);
    void computeMAC(const char* inBuf, size_t inLen, char* outBuf, size_t outBufSize, size_t& outLen);
    bool chunkAndUpload(const char* data, size_t len);
};
