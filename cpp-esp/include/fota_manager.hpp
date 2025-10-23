#pragma once

#include <Arduino.h>
#include <Update.h>
#include <FS.h>
#include <LittleFS.h>
#include <Crypto.h>
#include <SHA256.h>
#include <string>
#include <functional>
#include "http_client.hpp"
#include "security_layer.hpp"

/**
 * @brief FOTA (Firmware Over-The-Air) Manager
 * 
 * Implements secure firmware updates with:
 * - Chunked download across multiple communication intervals
 * - Integrity verification (SHA-256 hash)
 * - Authenticity verification (HMAC per chunk)
 * - Resumption after interruptions
 * - Rollback on verification/boot failure
 * - Detailed progress reporting
 * 
 * ESP32 OTA Partitions:
 * - factory: Original firmware (fallback)
 * - ota_0: First OTA partition
 * - ota_1: Second OTA partition
 * 
 * Update Flow:
 * 1. Download manifest
 * 2. Download chunks (with resume support)
 * 3. Verify hash
 * 4. Write to inactive OTA partition
 * 5. Set boot partition
 * 6. Reboot
 * 7. Verify boot success
 * 8. Report status to cloud
 */

// FOTA states
enum class FOTAState {
    IDLE,
    CHECKING_MANIFEST,
    DOWNLOADING,
    VERIFYING,
    WRITING,
    REBOOTING,
    BOOT_VERIFICATION,
    ROLLBACK,
    COMPLETED,
    FAILED
};

// FOTA manifest structure
struct FOTAManifest {
    std::string version;
    uint32_t size;
    std::string hash;  // SHA-256 hex
    uint32_t chunk_size;
    uint32_t total_chunks;
    bool valid;

    FOTAManifest() : size(0), chunk_size(0), total_chunks(0), valid(false) {}
};

// FOTA progress structure
struct FOTAProgress {
    FOTAState state;
    uint32_t chunks_received;
    uint32_t total_chunks;
    uint32_t bytes_received;
    uint32_t total_bytes;
    bool verified;
    std::string error_message;
    std::string current_version;
    std::string new_version;

    FOTAProgress() 
        : state(FOTAState::IDLE)
        , chunks_received(0)
        , total_chunks(0)
        , bytes_received(0)
        , total_bytes(0)
        , verified(false) {}
};

class FOTAManager {
public:
    /**
     * @brief Constructor
     * @param http HTTP client for downloading chunks (FOTA endpoints don't use security)
     * @param security Security layer for HMAC verification
     * @param cloud_base_url Base URL for cloud API (e.g., "http://10.50.126.183:8080")
     */
    FOTAManager(EcoHttpClient* http, SecurityLayer* security, const std::string& cloud_base_url);
    ~FOTAManager();

    /**
     * @brief Initialize FOTA manager
     * Must be called after WiFi and security are initialized
     */
    bool begin();

    /**
     * @brief Check for available firmware updates
     * @return true if new firmware is available
     */
    bool checkForUpdate();

    /**
     * @brief Start firmware download
     * @return true if download started successfully
     */
    bool startDownload();

    /**
     * @brief Process one chunk download (non-blocking)
     * Call this periodically in the main loop
     * @return true if chunk processed successfully, false if error or complete
     */
    bool processChunk();

    /**
     * @brief Verify downloaded firmware integrity
     * @return true if verification passed
     */
    bool verifyFirmware();

    /**
     * @brief Apply firmware update and reboot
     * This will set the boot partition and restart the device
     * @return true if update applied successfully (won't return on success)
     */
    bool applyUpdate();

    /**
     * @brief Trigger rollback to previous firmware
     * @param reason Reason for rollback
     * @return true if rollback initiated
     */
    bool rollback(const std::string& reason);

    /**
     * @brief Report progress to cloud
     * @param force Force report even if not at reporting interval
     * @return true if report sent successfully
     */
    bool reportProgress(bool force = false);

    /**
     * @brief Report boot status to cloud (call in setup after boot)
     * @return true if report sent successfully
     */
    bool reportBootStatus();

    /**
     * @brief Get current FOTA progress
     */
    FOTAProgress getProgress() const { return progress_; }

    /**
     * @brief Get current FOTA state
     */
    FOTAState getState() const { return progress_.state; }

    /**
     * @brief Check if FOTA is in progress
     */
    bool isInProgress() const {
        return progress_.state != FOTAState::IDLE && 
               progress_.state != FOTAState::COMPLETED &&
               progress_.state != FOTAState::FAILED;
    }

    /**
     * @brief Cancel ongoing FOTA operation
     */
    void cancel();

    /**
     * @brief Reset FOTA state
     */
    void reset();

    /**
     * @brief Loop function - call periodically
     * Handles automatic chunk downloads and progress reporting
     */
    void loop();

private:
    // HTTP client (FOTA endpoints don't require security)
    EcoHttpClient* http_;
    
    // Security layer for HMAC
    SecurityLayer* security_;
    
    // Cloud API base URL
    std::string cloud_base_url_;
    
    // FOTA manifest
    FOTAManifest manifest_;
    
    // Progress tracking
    FOTAProgress progress_;
    
    // Downloaded chunks tracking
    std::vector<bool> chunks_downloaded_;
    
    // SHA-256 for integrity
    SHA256 sha256_;
    
    // Firmware buffer for hash calculation
    static constexpr size_t HASH_BUFFER_SIZE = 4096;
    
    // State persistence file
    static constexpr const char* STATE_FILE = "/fota_state.json";
    // REMOVED: FIRMWARE_FILE - no longer needed, writing directly to OTA partition
    static constexpr const char* BOOT_COUNT_FILE = "/boot_count.txt";
    
    // Maximum boot attempts before rollback
    static constexpr uint8_t MAX_BOOT_ATTEMPTS = 3;
    
    // Progress reporting interval (ms)
    static constexpr uint32_t REPORT_INTERVAL_MS = 5000;
    unsigned long last_report_ms_;
    
    // Helper functions
    bool fetchManifest();
    bool fetchChunk(uint32_t chunk_number);
    bool saveState();
    bool loadState();
    bool saveFirmwareChunk(uint32_t chunk_number, const uint8_t* data, size_t size);
    // REMOVED: loadFirmwareForVerification() and clearFirmwareFile() - no longer needed
    void setState(FOTAState state, const std::string& error = "");
    bool verifyChunkHMAC(const uint8_t* data, size_t size, const std::string& mac);
    int getBootCount();
    void incrementBootCount();
    void clearBootCount();
    std::string getCurrentFirmwareVersion();
    void logFOTAEvent(const std::string& event_type, const std::string& details);
};
