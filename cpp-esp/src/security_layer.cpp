#include "../include/security_layer.hpp"
#include "../include/logger.hpp"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

// Include cryptographic libraries
#ifdef ESP32
    #include <mbedtls/md.h>
    #include <mbedtls/aes.h>
    #include <mbedtls/base64.h>
#else
    // For ESP8266, use BearSSL or Crypto library
    #include <bearssl/bearssl_hash.h>
    #include <bearssl/bearssl_hmac.h>
    #include <Crypto.h>
    #include <AES.h>
    #include <CBC.h>
    #include <SHA256.h>
#endif

#include <ArduinoJson.h>
#include <cstring>
#include <algorithm>

// ============================================================================
// Constructor & Initialization
// ============================================================================

SecurityLayer::SecurityLayer(const SecurityConfig& config)
    : config_(config)
    , current_nonce_(1)
    , last_received_nonce_(0)
    , messages_secured_(0)
    , messages_verified_(0)
    , replay_attempts_(0)
    , mac_failures_(0)
    , mutex_initialized_(false) {
    
    // Reserve space for nonce history
    recent_nonces_.reserve(MAX_NONCE_HISTORY);
}

SecurityLayer::~SecurityLayer() {
    end();
}

bool SecurityLayer::begin() {
    Logger::info("[Security] Initializing security layer...");
    
    // Validate PSK
    if (config_.psk.empty() || config_.psk.length() != PSK_SIZE * 2) {
        Logger::error("[Security] Invalid PSK length: %u (expected %u hex chars)", 
                     (unsigned)config_.psk.length(), (unsigned)(PSK_SIZE * 2));
        return false;
    }
    
    // Initialize cryptographic subsystem
    if (!initializeCrypto()) {
        Logger::error("[Security] Failed to initialize crypto subsystem");
        return false;
    }
    
    // Load nonce state from persistent storage
    bool state_loaded = loadNonceState();
    if (!state_loaded) {
        Logger::warn("[Security] Failed to load nonce state from file");
        
        // Use smart recovery strategy to avoid nonce conflicts
        uint32_t recovery_nonce = estimateRecoveryNonce();
        if (recovery_nonce > current_nonce_) {
            current_nonce_ = recovery_nonce;
            Logger::info("[Security] Applied recovery nonce: %u", current_nonce_);
        }
        
        // Reset received nonce tracking for fresh synchronization
        last_received_nonce_ = 0;
        recent_nonces_.clear();
        
        Logger::warn("[Security] Using recovery mode - ready for server sync");
    }
    
    Logger::info("[Security] Security layer initialized");
    Logger::info("[Security]   Encryption: %s (%s)", 
                config_.encryption_enabled ? "enabled" : "disabled",
                config_.use_real_encryption ? "AES-CBC" : "base64-simulated");
    Logger::info("[Security]   Anti-replay: enabled (window=%u)", config_.nonce_window);
    Logger::info("[Security]   Current nonce: %u", current_nonce_);
    Logger::info("[Security]   Last received: %u", last_received_nonce_);
    Logger::info("[Security]   State source: %s", state_loaded ? "FILE" : "RECOVERY");
    
    // Save current state immediately to ensure persistence
    if (saveNonceState()) {
        Logger::debug("[Security] Initial state saved to file");
    } else {
        Logger::warn("[Security] Failed to save initial state");
    }
    
    return true;
}

void SecurityLayer::end() {
    // Save nonce state
    saveNonceState();
    
    // Cleanup
    cleanupCrypto();
    
    Logger::info("[Security] Security layer shutdown");
}

// ============================================================================
// Outgoing Message Protection
// ============================================================================

SecurityResult SecurityLayer::secureMessage(const std::string& plain_payload,
                                           SecuredMessage& secured_msg) {
    SecurityResult result;
    result.status = SecurityStatus::SUCCESS;
    
    Logger::debug("[Security] Securing message: %u bytes", (unsigned)plain_payload.length());
    
    // Generate nonce
    secured_msg.nonce = getNextNonce();
    secured_msg.timestamp = millis();
    secured_msg.encrypted = config_.encryption_enabled;
    
    // Encrypt or encode payload
    std::string processed_payload;
    if (config_.encryption_enabled) {
        if (config_.use_real_encryption) {
            // Use AES-CBC encryption
            if (!encryptAES(plain_payload, processed_payload)) {
                result.status = SecurityStatus::ENCRYPTION_ERROR;
                result.error_message = "AES encryption failed";
                Logger::error("[Security] %s", result.error_message.c_str());
                return result;
            }
            Logger::debug("[Security] AES encryption successful: %u -> %u bytes",
                         (unsigned)plain_payload.length(), 
                         (unsigned)processed_payload.length());
        } else {
            // Simulate encryption with base64
            processed_payload = simulateEncryption(plain_payload);
            Logger::debug("[Security] Base64 simulation: %u -> %u bytes",
                         (unsigned)plain_payload.length(), 
                         (unsigned)processed_payload.length());
        }
    } else {
        // No encryption, just base64 encode for transport
        processed_payload = simulateEncryption(plain_payload);
    }
    
    secured_msg.payload = processed_payload;
    
    // Compute HMAC over: nonce + timestamp + encrypted_flag + payload
    // Pre-allocate to avoid stack pressure from string operations
    std::string hmac_input;
    hmac_input.reserve(20 + 20 + 1 + secured_msg.payload.length()); // nonce + timestamp + flag + payload
    hmac_input.append(std::to_string(secured_msg.nonce));
    hmac_input.append(std::to_string(secured_msg.timestamp));
    hmac_input.append(secured_msg.encrypted ? "1" : "0");
    hmac_input.append(secured_msg.payload);
    
    secured_msg.mac = computeHMAC(hmac_input, config_.psk);
    
    if (secured_msg.mac.empty()) {
        result.status = SecurityStatus::KEY_ERROR;
        result.error_message = "HMAC computation failed";
        Logger::error("[Security] %s", result.error_message.c_str());
        return result;
    }
    
    messages_secured_++;
    
    Logger::debug("[Security] Message secured: nonce=%u, mac=%s...", 
                 secured_msg.nonce, secured_msg.mac.substr(0, 8).c_str());
    
    return result;
}

std::string SecurityLayer::generateSecuredEnvelope(const SecuredMessage& secured_msg) {
    DynamicJsonDocument doc(4096);  // Allocate on heap instead of stack
    
    doc["nonce"] = secured_msg.nonce;
    doc["timestamp"] = secured_msg.timestamp;
    doc["encrypted"] = secured_msg.encrypted;
    doc["payload"] = secured_msg.payload;
    doc["mac"] = secured_msg.mac;
    
    std::string result;
    serializeJson(doc, result);
    
    Logger::debug("[Security] Generated envelope: %u bytes", (unsigned)result.length());
    
    return result;
}

// ============================================================================
// Incoming Message Verification
// ============================================================================

SecurityResult SecurityLayer::verifyMessage(const std::string& secured_json,
                                           std::string& plain_payload) {
    SecurityResult result;
    result.status = SecurityStatus::SUCCESS;
    
    Logger::debug("[Security] Verifying message: %u bytes", (unsigned)secured_json.length());
    
    // Parse secured envelope
    SecuredMessage secured_msg;
    if (!parseSecuredEnvelope(secured_json, secured_msg)) {
        result.status = SecurityStatus::INVALID_FORMAT;
        result.error_message = "Failed to parse secured envelope";
        Logger::error("[Security] %s", result.error_message.c_str());
        return result;
    }
    
    // Check nonce for replay attack
    if (!isNonceValid(secured_msg.nonce)) {
        replay_attempts_++;
        result.status = SecurityStatus::REPLAY_DETECTED;
        result.error_message = "Nonce replay detected: " + std::to_string(secured_msg.nonce);
        Logger::warn("[Security] %s", result.error_message.c_str());
        return result;
    }
    
    // Verify HMAC
    // Pre-allocate to avoid stack pressure from string operations
    std::string hmac_input;
    hmac_input.reserve(20 + 20 + 1 + secured_msg.payload.length()); // nonce + timestamp + flag + payload
    hmac_input.append(std::to_string(secured_msg.nonce));
    hmac_input.append(std::to_string(secured_msg.timestamp));
    hmac_input.append(secured_msg.encrypted ? "1" : "0");
    hmac_input.append(secured_msg.payload);
    
    if (!verifyHMAC(hmac_input, secured_msg.mac, config_.psk)) {
        mac_failures_++;
        result.status = SecurityStatus::INVALID_MAC;
        result.error_message = "HMAC verification failed";
        Logger::error("[Security] %s for nonce %u", result.error_message.c_str(), 
                     secured_msg.nonce);
        return result;
    }
    
    // Update nonce tracking
    updateLastNonce(secured_msg.nonce);
    
    // Decrypt or decode payload
    if (secured_msg.encrypted) {
        if (config_.use_real_encryption) {
            // Use AES-CBC decryption
            if (!decryptAES(secured_msg.payload, plain_payload)) {
                result.status = SecurityStatus::ENCRYPTION_ERROR;
                result.error_message = "AES decryption failed";
                Logger::error("[Security] %s", result.error_message.c_str());
                return result;
            }
        } else {
            // Simulate decryption with base64
            plain_payload = simulateDecryption(secured_msg.payload);
        }
    } else {
        // No encryption, just decode
        plain_payload = simulateDecryption(secured_msg.payload);
    }
    
    messages_verified_++;
    
    Logger::debug("[Security] Message verified: nonce=%u, payload=%u bytes",
                 secured_msg.nonce, (unsigned)plain_payload.length());
    
    return result;
}

bool SecurityLayer::parseSecuredEnvelope(const std::string& secured_json,
                                        SecuredMessage& secured_msg) {
    DynamicJsonDocument doc(4096);  // Allocate on heap instead of stack
    DeserializationError error = deserializeJson(doc, secured_json);
    
    if (error) {
        Logger::error("[Security] JSON parse error: %s", error.c_str());
        return false;
    }
    
    // Validate required fields
    if (!doc.containsKey("nonce") || !doc.containsKey("payload") || 
        !doc.containsKey("mac")) {
        Logger::error("[Security] Missing required fields in secured envelope");
        return false;
    }
    
    secured_msg.nonce = doc["nonce"].as<uint32_t>();
    secured_msg.timestamp = doc.containsKey("timestamp") ? 
                           doc["timestamp"].as<uint32_t>() : 0;
    secured_msg.encrypted = doc.containsKey("encrypted") ? 
                           doc["encrypted"].as<bool>() : false;
    secured_msg.payload = doc["payload"].as<std::string>();
    secured_msg.mac = doc["mac"].as<std::string>();
    
    return true;
}

// ============================================================================
// Nonce Management
// ============================================================================

uint32_t SecurityLayer::getNextNonce() {
    lock();
    uint32_t nonce = current_nonce_++;
    unlock();
    
    // Persist periodically (every 10 nonces to reduce flash wear)
    if (nonce % 10 == 0) {
        saveNonceState();
    }
    
    return nonce;
}

bool SecurityLayer::isNonceValid(uint32_t nonce) {
    lock();
    
    // Check if nonce already seen (exact replay)
    for (uint32_t seen : recent_nonces_) {
        if (seen == nonce) {
            unlock();
            Logger::warn("[Security] Exact nonce replay: %u", nonce);
            return false;
        }
    }
    
    // Check nonce window (if strict checking enabled)
    if (config_.strict_nonce_checking) {
        if (nonce <= last_received_nonce_) {
            unlock();
            Logger::warn("[Security] Nonce too old: %u (last: %u)", 
                        nonce, last_received_nonce_);
            return false;
        }
        
        // Special case: If we've never received a valid nonce (last_received_nonce_ == 0),
        // be more lenient to allow initial synchronization
        if (last_received_nonce_ == 0) {
            // For first-time sync, allow any reasonable nonce (up to 1000)
            if (nonce > 1000) {
                unlock();
                Logger::warn("[Security] First nonce too high: %u (max: 1000 for initial sync)", nonce);
                return false;
            }
            // Accept the first valid nonce regardless of window
            unlock();
            Logger::info("[Security] Accepting first nonce for initial sync: %u", nonce);
            return true;
        }
        
        if (nonce > last_received_nonce_ + config_.nonce_window) {
            unlock();
            Logger::warn("[Security] Nonce too far ahead: %u (last: %u, window: %u)",
                        nonce, last_received_nonce_, config_.nonce_window);
            return false;
        }
    }
    
    unlock();
    return true;
}

void SecurityLayer::updateLastNonce(uint32_t nonce) {
    lock();
    
    // Update last seen nonce
    if (nonce > last_received_nonce_) {
        last_received_nonce_ = nonce;
    }
    
    // Add to recent nonces history
    recent_nonces_.push_back(nonce);
    
    // Maintain history size
    if (recent_nonces_.size() > MAX_NONCE_HISTORY) {
        recent_nonces_.erase(recent_nonces_.begin());
    }
    
    unlock();
}

bool SecurityLayer::loadNonceState() {
    if (!LittleFS.begin()) {
        Logger::error("[Security] Failed to mount LittleFS for nonce state");
        return false;
    }
    
    File file = LittleFS.open(NONCE_FILE, "r");
    if (!file) {
        Logger::warn("[Security] Nonce state file '%s' not found", NONCE_FILE);
        return false;
    }
    
    size_t file_size = file.size();
    Logger::debug("[Security] Loading nonce state from file (size: %u bytes)", (unsigned)file_size);
    
    // Minimum expected size: version(4) + current(4) + last_received(4) + history_count(4) = 16 bytes
    if (file_size < 16) {
        Logger::warn("[Security] Nonce state file too small (%u bytes), may be corrupted", (unsigned)file_size);
        file.close();
        return false;
    }
    
    // Read nonce state: version (4) + current_nonce (4) + last_received (4)
    uint32_t version = 0;
    size_t bytes_read = file.read((uint8_t*)&version, sizeof(uint32_t));
    
    if (bytes_read != sizeof(uint32_t)) {
        Logger::warn("[Security] Failed to read version from nonce state file");
        file.close();
        return false;
    }
    
    if (version != 1) {
        Logger::warn("[Security] Incompatible nonce state version: %u (expected: 1)", version);
        file.close();
        return false;
    }
    
    // Read nonce values with error checking
    bytes_read = file.read((uint8_t*)&current_nonce_, sizeof(uint32_t));
    if (bytes_read != sizeof(uint32_t)) {
        Logger::warn("[Security] Failed to read current_nonce from state file");
        file.close();
        return false;
    }
    
    bytes_read = file.read((uint8_t*)&last_received_nonce_, sizeof(uint32_t));
    if (bytes_read != sizeof(uint32_t)) {
        Logger::warn("[Security] Failed to read last_received_nonce from state file");
        file.close();
        return false;
    }
    
    // Read nonce history count
    uint32_t history_count = 0;
    bytes_read = file.read((uint8_t*)&history_count, sizeof(uint32_t));
    if (bytes_read != sizeof(uint32_t)) {
        Logger::warn("[Security] Failed to read history_count from state file");
        file.close();
        return false;
    }
    
    // Sanity check history count
    if (history_count > MAX_NONCE_HISTORY) {
        Logger::warn("[Security] History count too large: %u (max: %u)", history_count, MAX_NONCE_HISTORY);
        history_count = MAX_NONCE_HISTORY;
    }
    
    recent_nonces_.clear();
    for (uint32_t i = 0; i < history_count; i++) {
        uint32_t nonce;
        bytes_read = file.read((uint8_t*)&nonce, sizeof(uint32_t));
        if (bytes_read == sizeof(uint32_t)) {
            recent_nonces_.push_back(nonce);
        } else {
            Logger::warn("[Security] Failed to read nonce history item %u", i);
            break;
        }
    }
    
    file.close();
    
    Logger::info("[Security] Loaded nonce state: current=%u, last_received=%u, history=%u",
                current_nonce_, last_received_nonce_, (unsigned)recent_nonces_.size());
    
    return true;
}

bool SecurityLayer::saveNonceState() {
    if (!LittleFS.begin()) {
        Logger::error("[Security] Failed to mount LittleFS");
        return false;
    }
    
    // Create directory if needed
    if (!LittleFS.exists("/security")) {
        LittleFS.mkdir("/security");
    }
    
    File file = LittleFS.open(NONCE_FILE, "w");
    if (!file) {
        Logger::error("[Security] Failed to open nonce state file for writing");
        return false;
    }
    
    // Write nonce state
    uint32_t version = 1;
    file.write((uint8_t*)&version, sizeof(uint32_t));
    file.write((uint8_t*)&current_nonce_, sizeof(uint32_t));
    file.write((uint8_t*)&last_received_nonce_, sizeof(uint32_t));
    
    // Write nonce history
    uint32_t history_count = recent_nonces_.size();
    file.write((uint8_t*)&history_count, sizeof(uint32_t));
    
    for (uint32_t nonce : recent_nonces_) {
        file.write((uint8_t*)&nonce, sizeof(uint32_t));
    }
    
    file.close();
    
    Logger::debug("[Security] Saved nonce state: current=%u, last_received=%u, history=%u",
                 current_nonce_, last_received_nonce_, (unsigned)recent_nonces_.size());
    
    return true;
}

uint32_t SecurityLayer::estimateRecoveryNonce() {
    // Smart recovery strategy: estimate a safe nonce value when state loading fails
    
    // Try to read any backup or partial state indicators
    uint32_t recovery_base = current_nonce_; // Start with constructor default (1)
    
    // Strategy 1: Use time-based estimation if available
    #ifdef ESP32
    unsigned long uptime_ms = millis();
    if (uptime_ms > 60000) { // If we've been running for more than 1 minute
        // Estimate based on typical usage patterns - device usually makes requests every ~30 seconds
        // So after 1 minute of uptime, we might have made 2-3 requests
        uint32_t estimated_requests = (uptime_ms / 30000) + 1; // Conservative estimate
        recovery_base = std::max(recovery_base, estimated_requests);
    }
    #endif
    
    // Strategy 2: Use a safe offset to avoid immediate conflicts
    // Add a buffer based on typical session length
    recovery_base += 50; // Safe buffer to avoid conflicts with recent server nonces
    
    // Strategy 3: Ensure we don't go backwards if we already have a reasonable value
    if (current_nonce_ > 100) {
        recovery_base = current_nonce_ + 10; // Continue from current with small buffer
    }
    
    Logger::info("[Security] Estimated recovery nonce: %u (base=%u)", recovery_base, current_nonce_);
    return recovery_base;
}

// ============================================================================
// Cryptographic Operations
// ============================================================================

std::string SecurityLayer::computeHMAC(const std::string& data, const std::string& key) {
    uint8_t hmac_result[HMAC_SIZE];
    
#ifdef ESP32
    // Use mbedtls for ESP32
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    
    // Convert hex key to bytes
    uint8_t key_bytes[PSK_SIZE];
    if (!hexToBytes(key, key_bytes, PSK_SIZE)) {
        Logger::error("[Security] Failed to convert key from hex");
        mbedtls_md_free(&ctx);
        return "";
    }
    
    mbedtls_md_hmac_starts(&ctx, key_bytes, PSK_SIZE);
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)data.c_str(), data.length());
    mbedtls_md_hmac_finish(&ctx, hmac_result);
    
    mbedtls_md_free(&ctx);
    
#else
    // Use BearSSL/Crypto for ESP8266
    uint8_t key_bytes[PSK_SIZE];
    if (!hexToBytes(key, key_bytes, PSK_SIZE)) {
        Logger::error("[Security] Failed to convert key from hex");
        return "";
    }
    
    // Use SHA256 HMAC
    SHA256 sha256;
    sha256.resetHMAC(key_bytes, PSK_SIZE);
    sha256.update((const uint8_t*)data.c_str(), data.length());
    sha256.finalizeHMAC(key_bytes, PSK_SIZE, hmac_result, HMAC_SIZE);
#endif
    
    return bytesToHex(hmac_result, HMAC_SIZE);
}

bool SecurityLayer::verifyHMAC(const std::string& data, const std::string& mac,
                              const std::string& key) {
    std::string computed_mac = computeHMAC(data, key);
    
    if (computed_mac.empty()) {
        return false;
    }
    
    // Constant-time comparison to prevent timing attacks
    if (computed_mac.length() != mac.length()) {
        return false;
    }
    
    uint8_t diff = 0;
    for (size_t i = 0; i < mac.length(); i++) {
        diff |= (computed_mac[i] ^ mac[i]);
    }
    
    return diff == 0;
}

bool SecurityLayer::encryptAES(const std::string& plain_data, std::string& encrypted_data) {
    // Derive AES key and IV from PSK
    uint8_t key[AES_KEY_SIZE];
    uint8_t iv[AES_BLOCK_SIZE];
    
    if (!hexToBytes(config_.psk, key, AES_KEY_SIZE)) {
        Logger::error("[Security] Failed to derive AES key from PSK");
        return false;
    }
    
    // Simple IV derivation (in production, use proper KDF)
    memset(iv, 0, AES_BLOCK_SIZE);
    memcpy(iv, key, AES_BLOCK_SIZE);
    
    // Add PKCS#7 padding
    size_t padded_len = ((plain_data.length() / AES_BLOCK_SIZE) + 1) * AES_BLOCK_SIZE;
    uint8_t pad_value = padded_len - plain_data.length();
    
    std::vector<uint8_t> padded_plain(padded_len);
    memcpy(padded_plain.data(), plain_data.c_str(), plain_data.length());
    memset(padded_plain.data() + plain_data.length(), pad_value, pad_value);
    
    // Encrypt
    std::vector<uint8_t> cipher(padded_len);
    size_t cipher_len = 0;
    
    if (!aesEncryptCBC(padded_plain.data(), padded_len, 
                      cipher.data(), cipher_len, key, iv)) {
        Logger::error("[Security] AES encryption failed");
        return false;
    }
    
    // Base64 encode result
    encrypted_data = base64Encode(cipher.data(), cipher_len);
    
    return true;
}

bool SecurityLayer::decryptAES(const std::string& encrypted_data, std::string& plain_data) {
    // Derive AES key and IV from PSK
    uint8_t key[AES_KEY_SIZE];
    uint8_t iv[AES_BLOCK_SIZE];
    
    if (!hexToBytes(config_.psk, key, AES_KEY_SIZE)) {
        Logger::error("[Security] Failed to derive AES key from PSK");
        return false;
    }
    
    // Simple IV derivation
    memset(iv, 0, AES_BLOCK_SIZE);
    memcpy(iv, key, AES_BLOCK_SIZE);
    
    // Base64 decode
    std::vector<uint8_t> cipher;
    if (!base64Decode(encrypted_data, cipher)) {
        Logger::error("[Security] Base64 decode failed");
        return false;
    }
    
    // Decrypt
    std::vector<uint8_t> plain(cipher.size());
    size_t plain_len = 0;
    
    if (!aesDecryptCBC(cipher.data(), cipher.size(),
                      plain.data(), plain_len, key, iv)) {
        Logger::error("[Security] AES decryption failed");
        return false;
    }
    
    // Remove PKCS#7 padding
    if (plain_len > 0) {
        uint8_t pad_value = plain[plain_len - 1];
        if (pad_value > 0 && pad_value <= AES_BLOCK_SIZE) {
            plain_len -= pad_value;
        }
    }
    
    plain_data = std::string((char*)plain.data(), plain_len);
    
    return true;
}

std::string SecurityLayer::simulateEncryption(const std::string& plain_data) {
    return base64Encode((const uint8_t*)plain_data.c_str(), plain_data.length());
}

std::string SecurityLayer::simulateDecryption(const std::string& encoded_data) {
    std::vector<uint8_t> decoded;
    if (!base64Decode(encoded_data, decoded)) {
        Logger::error("[Security] Base64 decode failed in simulation");
        return "";
    }
    return std::string((char*)decoded.data(), decoded.size());
}

// ============================================================================
// Configuration
// ============================================================================

void SecurityLayer::updateConfig(const SecurityConfig& config) {
    lock();
    config_ = config;
    unlock();
    
    Logger::info("[Security] Configuration updated");
}

SecurityConfig SecurityLayer::getConfig() const {
    return config_;
}

bool SecurityLayer::updatePSK(const std::string& new_psk) {
    if (new_psk.length() != PSK_SIZE * 2) {
        Logger::error("[Security] Invalid PSK length");
        return false;
    }
    
    lock();
    config_.psk = new_psk;
    unlock();
    
    Logger::info("[Security] PSK updated");
    return true;
}

// ============================================================================
// Statistics & Diagnostics
// ============================================================================

std::string SecurityLayer::getSecurityStats() const {
    DynamicJsonDocument doc(512);  // Allocate on heap instead of stack
    
    doc["messages_secured"] = messages_secured_;
    doc["messages_verified"] = messages_verified_;
    doc["replay_attempts"] = replay_attempts_;
    doc["mac_failures"] = mac_failures_;
    doc["current_nonce"] = current_nonce_;
    doc["last_received_nonce"] = last_received_nonce_;
    doc["nonce_history_size"] = recent_nonces_.size();
    
    std::string result;
    serializeJson(doc, result);
    
    return result;
}

void SecurityLayer::resetStats() {
    lock();
    messages_secured_ = 0;
    messages_verified_ = 0;
    replay_attempts_ = 0;
    mac_failures_ = 0;
    unlock();
    
    Logger::info("[Security] Statistics reset");
}

// ============================================================================
// Internal Helpers
// ============================================================================

bool SecurityLayer::initializeCrypto() {
    // Platform-specific crypto initialization
#ifdef ESP32
    // mbedtls is built-in on ESP32
    Logger::debug("[Security] Using mbedtls (ESP32)");
#else
    // BearSSL/Crypto for ESP8266
    Logger::debug("[Security] Using BearSSL/Crypto (ESP8266)");
#endif
    
    mutex_initialized_ = true;
    return true;
}

void SecurityLayer::cleanupCrypto() {
    // Cleanup if needed
    mutex_initialized_ = false;
}

std::string SecurityLayer::bytesToHex(const uint8_t* bytes, size_t len) const {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(len * 2);
    
    for (size_t i = 0; i < len; i++) {
        result.push_back(hex_chars[(bytes[i] >> 4) & 0x0F]);
        result.push_back(hex_chars[bytes[i] & 0x0F]);
    }
    
    return result;
}

bool SecurityLayer::hexToBytes(const std::string& hex, uint8_t* bytes, size_t max_len) const {
    if (hex.length() != max_len * 2) {
        return false;
    }
    
    for (size_t i = 0; i < max_len; i++) {
        char high = hex[i * 2];
        char low = hex[i * 2 + 1];
        
        uint8_t high_val = (high >= '0' && high <= '9') ? (high - '0') :
                          (high >= 'a' && high <= 'f') ? (high - 'a' + 10) :
                          (high >= 'A' && high <= 'F') ? (high - 'A' + 10) : 0xFF;
        
        uint8_t low_val = (low >= '0' && low <= '9') ? (low - '0') :
                         (low >= 'a' && low <= 'f') ? (low - 'a' + 10) :
                         (low >= 'A' && low <= 'F') ? (low - 'A' + 10) : 0xFF;
        
        if (high_val == 0xFF || low_val == 0xFF) {
            return false;
        }
        
        bytes[i] = (high_val << 4) | low_val;
    }
    
    return true;
}

std::string SecurityLayer::base64Encode(const uint8_t* data, size_t len) const {
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    // Pre-allocate to avoid multiple reallocations (fixes stack overflow)
    result.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    uint8_t array_3[3];
    uint8_t array_4[4];
    
    while (len--) {
        array_3[i++] = *(data++);
        if (i == 3) {
            array_4[0] = (array_3[0] & 0xfc) >> 2;
            array_4[1] = ((array_3[0] & 0x03) << 4) + ((array_3[1] & 0xf0) >> 4);
            array_4[2] = ((array_3[1] & 0x0f) << 2) + ((array_3[2] & 0xc0) >> 6);
            array_4[3] = array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++) {
                result += base64_chars[array_4[i]];
            }
            i = 0;
        }
    }
    
    if (i) {
        for (size_t j = i; j < 3; j++) {
            array_3[j] = '\0';
        }
        
        array_4[0] = (array_3[0] & 0xfc) >> 2;
        array_4[1] = ((array_3[0] & 0x03) << 4) + ((array_3[1] & 0xf0) >> 4);
        array_4[2] = ((array_3[1] & 0x0f) << 2) + ((array_3[2] & 0xc0) >> 6);
        
        for (size_t j = 0; j < i + 1; j++) {
            result += base64_chars[array_4[j]];
        }
        
        while (i++ < 3) {
            result += '=';
        }
    }
    
    return result;
}

bool SecurityLayer::base64Decode(const std::string& encoded, std::vector<uint8_t>& decoded) const {
    static const uint8_t base64_table[256] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    };
    
    size_t in_len = encoded.size();
    size_t i = 0;
    size_t j = 0;
    int in_ = 0;
    uint8_t char_array_4[4], char_array_3[3];
    
    decoded.clear();
    
    while (in_len-- && (encoded[in_] != '=') && 
           (isalnum(encoded[in_]) || (encoded[in_] == '+') || (encoded[in_] == '/'))) {
        char_array_4[i++] = encoded[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = base64_table[char_array_4[i]];
            }
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; i < 3; i++) {
                decoded.push_back(char_array_3[i]);
            }
            i = 0;
        }
    }
    
    if (i) {
        for (j = 0; j < i; j++) {
            char_array_4[j] = base64_table[char_array_4[j]];
        }
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        
        for (j = 0; j < i - 1; j++) {
            decoded.push_back(char_array_3[j]);
        }
    }
    
    return true;
}

bool SecurityLayer::aesEncryptCBC(const uint8_t* plain, size_t plain_len,
                                 uint8_t* cipher, size_t& cipher_len,
                                 const uint8_t* key, const uint8_t* iv) {
#ifdef ESP32
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, AES_KEY_SIZE * 8);
    
    uint8_t iv_copy[AES_BLOCK_SIZE];
    memcpy(iv_copy, iv, AES_BLOCK_SIZE);
    
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, plain_len,
                                    iv_copy, plain, cipher);
    
    mbedtls_aes_free(&aes);
    
    if (ret == 0) {
        cipher_len = plain_len;
        return true;
    }
    return false;
    
#else
    // Use Crypto library for ESP8266
    CBC<AES256> cbc;
    cbc.setKey(key, AES_KEY_SIZE);
    cbc.setIV(iv, AES_BLOCK_SIZE);
    cbc.encrypt(cipher, plain, plain_len);
    cipher_len = plain_len;
    return true;
#endif
}

bool SecurityLayer::aesDecryptCBC(const uint8_t* cipher, size_t cipher_len,
                                 uint8_t* plain, size_t& plain_len,
                                 const uint8_t* key, const uint8_t* iv) {
#ifdef ESP32
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key, AES_KEY_SIZE * 8);
    
    uint8_t iv_copy[AES_BLOCK_SIZE];
    memcpy(iv_copy, iv, AES_BLOCK_SIZE);
    
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipher_len,
                                    iv_copy, cipher, plain);
    
    mbedtls_aes_free(&aes);
    
    if (ret == 0) {
        plain_len = cipher_len;
        return true;
    }
    return false;
    
#else
    // Use Crypto library for ESP8266
    CBC<AES256> cbc;
    cbc.setKey(key, AES_KEY_SIZE);
    cbc.setIV(iv, AES_BLOCK_SIZE);
    cbc.decrypt(plain, cipher, cipher_len);
    plain_len = cipher_len;
    return true;
#endif
}

void SecurityLayer::lock() {
    // Simple mutex implementation (for FreeRTOS, use actual mutexes)
    // For single-threaded Arduino, this is a no-op
}

void SecurityLayer::unlock() {
    // Simple mutex implementation
}
