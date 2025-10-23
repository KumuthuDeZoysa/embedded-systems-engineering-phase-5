// src/uplink_packetizer.cpp

#include <Arduino.h> // for millis()
#include "freertos/FreeRTOS.h" // for pdMS_TO_TICKS
#include <FS.h>
#include <LittleFS.h>
#include <cstring>     // memcpy, memset
#include <cstddef>     // size_t
#include <cstdint>     // uint32_t, etc.
#include <string>      // std::string

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../include/ticker_fallback.hpp"
#include "../include/uplink_packetizer.hpp"
#include "../include/logger.hpp"

// ---------- File-local helpers ----------

static std::string bufToHex(const char* buf, size_t len) {
    static const char HEX_DIGITS[] = "0123456789ABCDEF";
    std::string out;
    for (size_t i = 0; i < len; ++i) {
        const unsigned char b = static_cast<unsigned char>(buf[i]);
        out.push_back(HEX_DIGITS[(b >> 4) & 0x0F]);
        out.push_back(HEX_DIGITS[b & 0x0F]);
    }
    return out;
}

// Prefer pdMS_TO_TICKS(ms) for sleeps; don't redefine portTICK_PERIOD_MS.

// ---------- UplinkPacketizer ----------

UplinkPacketizer* UplinkPacketizer::instance_ = nullptr;

UplinkPacketizer::UplinkPacketizer(DataStorage* storage, SecureHttpClient* secure_http)
    : uploadTicker_(uploadTaskWrapper, 60000),
      storage_(storage),
      secure_http_(secure_http) {
    instance_ = this;
}

UplinkPacketizer::~UplinkPacketizer() {
    end();
}

void UplinkPacketizer::begin(uint32_t interval_ms) {
    uploadInterval_ = interval_ms;
    uploadTicker_.interval(uploadInterval_);
    running_ = true;
    uploadTicker_.start();
}

void UplinkPacketizer::end() {
    uploadTicker_.stop();
    running_ = false;
}

void UplinkPacketizer::setCloudEndpoint(const std::string& url) {
    cloudUrl_ = url;
}

// If you keep RAM-only uploads, this can just return true.
bool UplinkPacketizer::drainOutbox_() {
    return true;
}

void UplinkPacketizer::uploadTaskWrapper() {
    if (instance_) {
        instance_->uploadTask();
    }
}

void UplinkPacketizer::uploadTask() {
    // 1) Pull recent samples from the ring buffer
    size_t cap = storage_ ? storage_->getBufferCapacity() : 0;
    if (cap == 0) return;

    constexpr size_t MAX_SAFE = 1024; // keep allocations bounded
    size_t toRead = (cap > MAX_SAFE) ? MAX_SAFE : cap;

    Sample* sampleBuf = static_cast<Sample*>(malloc(sizeof(Sample) * toRead));
    if (!sampleBuf) {
        Logger::warn("UplinkPacketizer: failed to allocate sampleBuf");
        return;
    }

    size_t sampleCount = storage_->readLastSamples(static_cast<int>(toRead), sampleBuf, toRead);
    Logger::info("[Uplink] sampleCount=%u", (unsigned)sampleCount);
    if (sampleCount == 0) {
        Logger::warn("[Uplink] No samples available for upload.");
        free(sampleBuf);
        return;
    }

    // --- Benchmarking ---
    // CPU time measurement not available; set to 0

    // 2) Compress samples: encode [timestamp, reg_addr, value] for every sample
    size_t outBufSize = sampleCount * (sizeof(uint32_t) + sizeof(uint8_t) + sizeof(float));
    Logger::debug("compressData called: sampleCount=%u, outBufSize=%u",
                  (unsigned)sampleCount, (unsigned)outBufSize);

    char* compressed = static_cast<char*>(malloc(outBufSize));
    if (!compressed) {
        Logger::warn("UplinkPacketizer: failed to allocate compressed buffer");
        free(sampleBuf);
        return;
    }

    size_t outIdx = 0;
    auto write = [&](const void* data, size_t len) -> bool {
        if (outIdx + len > outBufSize) return false;
        memcpy(compressed + outIdx, data, len);
        outIdx += len;
        return true;
    };

    for (size_t i = 0; i < sampleCount; ++i) {
        if (!write(&sampleBuf[i].timestamp, sizeof(uint32_t)) ||
            !write(&sampleBuf[i].reg_addr,  sizeof(uint8_t))  ||
            !write(&sampleBuf[i].value,     sizeof(float))) {
            Logger::warn("UplinkPacketizer: failed to write sample %u", static_cast<unsigned>(i));
            free(sampleBuf);
            free(compressed);
            return;
        }
    }

    size_t compLen = outIdx;
    uint32_t cpu_time = 0;

    Logger::info("[Uplink] Compressed payload length: %u", (unsigned)compLen);

    // Print first 24 bytes for backend cross-check
    size_t debugLen = (compLen < 24) ? compLen : 24;
    Logger::info("[Uplink] Compressed payload first 24 bytes: %s",
                 bufToHex(compressed, debugLen).c_str());

    // Optional: full payload hex (careful: large logs)
    Logger::debug("Compressed payload (%u bytes): %s",
                  static_cast<unsigned>(compLen),
                  bufToHex(compressed, compLen).c_str());

    // --- Lossless Recovery Verification ---
    bool lossless = true;
    for (size_t i = 0; i < sampleCount; ++i) {
        size_t idx = i * (sizeof(uint32_t) + sizeof(uint8_t) + sizeof(float));
        uint32_t ts;
        uint8_t reg;
        float val;
        memcpy(&ts, compressed + idx, sizeof(uint32_t));
        memcpy(&reg, compressed + idx + sizeof(uint32_t), sizeof(uint8_t));
        memcpy(&val, compressed + idx + sizeof(uint32_t) + sizeof(uint8_t), sizeof(float));
        if (ts != sampleBuf[i].timestamp || reg != sampleBuf[i].reg_addr || val != sampleBuf[i].value) {
            lossless = false;
            break;
        }
    }
    Logger::info("[Uplink] Lossless recovery verification: %s", lossless ? "PASS" : "FAIL");

    // --- Aggregation (min/avg/max) ---
    float minVal = 0.0f, maxVal = 0.0f, sumVal = 0.0f;
    if (sampleCount > 0) {
        minVal = sampleBuf[0].value;
        maxVal = sampleBuf[0].value;
        for (size_t i = 0; i < sampleCount; ++i) {
            float v = sampleBuf[i].value;
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
            sumVal += v;
        }
    }
    float avgVal = (sampleCount > 0) ? (sumVal / static_cast<float>(sampleCount)) : 0.0f;

    // --- Benchmark Metadata ---
    size_t original_size = sampleCount * sizeof(Sample);
    float compression_ratio = (original_size > 0) ? (static_cast<float>(compLen) / static_cast<float>(original_size)) : 0.0f;
    
    std::string benchmarkJson = "{";
    benchmarkJson += "\"compression_method\": \"delta/time-series\",";
    benchmarkJson += "\"num_samples\": " + std::to_string(sampleCount) + ",";
    benchmarkJson += "\"original_size\": " + std::to_string(original_size) + ",";
    benchmarkJson += "\"compressed_size\": " + std::to_string(compLen) + ",";
    benchmarkJson += "\"compression_ratio\": " + std::to_string(compression_ratio) + ",";
    benchmarkJson += "\"cpu_time_ms\": " + std::to_string(cpu_time) + ",";
    benchmarkJson += "\"lossless\": " + std::string(lossless ? "true" : "false") + ",";
    benchmarkJson += "\"min\": " + std::to_string(minVal) + ",";
    benchmarkJson += "\"avg\": " + std::to_string(avgVal) + ",";
    benchmarkJson += "\"max\": " + std::to_string(maxVal) + "}";
    // Avoid logging full JSON to prevent stack issues
    Logger::info("[Uplink] Benchmark metadata created (%u bytes)", benchmarkJson.length());

    // 3) Encrypt (stub) — keep buffer >= compLen
    char* encrypted = static_cast<char*>(malloc(compLen));
    if (!encrypted) {
        Logger::warn("UplinkPacketizer: failed to allocate encrypted buffer");
        free(sampleBuf);
        free(compressed);
        return;
    }
    size_t encLen = 0;
    encryptData(compressed, compLen, encrypted, compLen, encLen);

    // 4) MAC (stub)
    constexpr size_t MAC_MAX = 64;
    char* mac = static_cast<char*>(malloc(MAC_MAX));
    if (!mac) {
        Logger::warn("UplinkPacketizer: failed to allocate mac buffer");
        free(sampleBuf);
        free(compressed);
        free(encrypted);
        return;
    }
    size_t macLen = 0;
    computeMAC(encrypted, encLen, mac, MAC_MAX, macLen);


    // First, upload benchmark metadata as JSON (using plain HTTP, no security needed for metadata)
    if (secure_http_ && !cloudUrl_.empty()) {
        EcoHttpClient* plain_http = secure_http_->getHttpClient();
        EcoHttpResponse metaResp = plain_http->post((cloudUrl_ + "/meta").c_str(), 
                                                     benchmarkJson.c_str(), 
                                                     benchmarkJson.length(),
                                                     "application/json");
        Logger::info("[Uplink] Benchmark metadata upload status: %d", metaResp.status_code);
    }
    // Log only first few bytes to avoid stack overflow
    Logger::info("[Uplink] Upload payload (%u bytes): %s...",
                 (unsigned)encLen, bufToHex(encrypted, encLen < 8 ? encLen : 8).c_str());
    (void)chunkAndUpload(encrypted, encLen);

    free(sampleBuf);
    free(compressed);
    free(encrypted);
    free(mac);
}

void UplinkPacketizer::encryptData(const char* inBuf, size_t inLen,
                                   char* outBuf, size_t outBufSize,
                                   size_t& outLen) {
    Logger::debug("encryptData called: inLen=%u, outBufSize=%u",
                  (unsigned)inLen, (unsigned)outBufSize);

    // Stub: copy through
    if (outBufSize >= inLen) {
        memcpy(outBuf, inBuf, inLen);
        outLen = inLen;
    } else {
        outLen = 0;
    }
}

void UplinkPacketizer::computeMAC(const char* /*inBuf*/, size_t inLen,
                                  char* /*outBuf*/, size_t outBufSize,
                                  size_t& outLen) {
    Logger::debug("computeMAC called: inLen=%u, outBufSize=%u",
                  (unsigned)inLen, (unsigned)outBufSize);
    // Stub: none
    outLen = 0;
}

bool UplinkPacketizer::chunkAndUpload(const char* data, size_t len) {
    Logger::debug("chunkAndUpload called: len=%u", (unsigned)len);

    if (!secure_http_ || cloudUrl_.empty()) {
        Logger::warn("chunkAndUpload: secure_http_ or cloudUrl_ not set");
        return false;
    }

    constexpr size_t CHUNK = 1024;
    size_t offset = 0;

    while (offset < len) {
        size_t thisChunk = (len - offset > CHUNK) ? CHUNK : (len - offset);

        int attempt = 0;
        constexpr int MAX_RETRIES = 3;
        bool success = false;

        Logger::info("[Uplink] Sending chunk: endpoint=%s, offset=%u, size=%u",
                     cloudUrl_.c_str(), (unsigned)offset, (unsigned)thisChunk);

        while (attempt < MAX_RETRIES && !success) {
            // Convert binary chunk to string for SecureHttpClient
            std::string chunk_data(data + offset, thisChunk);
            std::string plain_response;
            EcoHttpResponse resp = secure_http_->securePost(cloudUrl_.c_str(), chunk_data, plain_response);
            Logger::debug("[Uplink] Attempt %d: status_code=%d, success=%d",
                          attempt + 1, resp.status_code, resp.isSuccess());
            success = resp.isSuccess();
            ++attempt;

            if (!success && attempt < MAX_RETRIES && resp.status_code == 0) {
                Logger::warn("[Uplink] Connection failure, retrying...");
                // vTaskDelay(200); // Delay not available in this build context
            }
        }

        if (!success) {
            Logger::warn("UplinkPacketizer: failed to upload chunk at offset %u after %d attempts",
                         static_cast<unsigned>(offset), MAX_RETRIES);
            return false; // abort whole upload; caller will retry later
        }

        Logger::info("[Uplink] Chunk upload OK: offset=%u, size=%u",
                     (unsigned)offset, (unsigned)thisChunk);
        offset += thisChunk;
    }

    Logger::info("[Uplink] All chunks uploaded successfully, total_len=%u", (unsigned)len);
    return true;
}

void UplinkPacketizer::loop() {
    if (running_) {
        uploadTicker_.update();
    }
}
