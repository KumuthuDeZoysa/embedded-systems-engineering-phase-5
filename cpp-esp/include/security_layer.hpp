#pragma once

#include <stdint.h>
#include <string>
#include <vector>

/**
 * @file security_layer.hpp
 * @brief Lightweight security layer for EcoWatt Device
 * 
 * Implements:
 * - HMAC-SHA256 authentication and integrity
 * - Optional AES-CBC encryption (or base64 simulation)
 * - Anti-replay protection with nonce management
 * - Secure message envelope format
 * 
 * Milestone 4 Part 3: Security Implementation
 */

// Security configuration
struct SecurityConfig {
    std::string psk;                    // Pre-shared key (32 bytes hex)
    bool encryption_enabled;            // Enable encryption (true) or just HMAC (false)
    bool use_real_encryption;          // Use AES (true) or base64 simulation (false)
    uint32_t nonce_window;             // Anti-replay window size (e.g., 100)
    bool strict_nonce_checking;        // Reject out-of-sequence nonces
};

// Secured message envelope
struct SecuredMessage {
    uint32_t nonce;                    // Sequential nonce for anti-replay
    std::string payload;               // Base64-encoded payload (encrypted or plain)
    std::string mac;                   // HMAC-SHA256 tag (hex)
    uint32_t timestamp;                // Message timestamp
    bool encrypted;                    // Flag indicating encryption status
};

// Security operation result
enum class SecurityStatus {
    SUCCESS,
    INVALID_MAC,                       // HMAC verification failed
    REPLAY_DETECTED,                   // Nonce already seen
    NONCE_TOO_OLD,                     // Nonce outside acceptable window
    ENCRYPTION_ERROR,                  // Encryption/decryption failed
    INVALID_FORMAT,                    // Message format error
    KEY_ERROR                          // PSK error
};

struct SecurityResult {
    SecurityStatus status;
    std::string error_message;
    bool is_success() const { 
        return status == SecurityStatus::SUCCESS; 
    }
};

/**
 * @class SecurityLayer
 * @brief Main security handler for message protection
 * 
 * Provides methods to secure outgoing messages and verify incoming messages.
 * Thread-safe for concurrent access from multiple subsystems.
 */
class SecurityLayer {
public:
    /**
     * @brief Constructor
     * @param config Security configuration
     */
    explicit SecurityLayer(const SecurityConfig& config);
    ~SecurityLayer();
    
    /**
     * @brief Initialize security layer
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Shutdown and cleanup
     */
    void end();
    
    // ========== Outgoing Message Protection ==========
    
    /**
     * @brief Secure a message for transmission
     * @param plain_payload Plain JSON payload
     * @param secured_msg Output secured message
     * @return Security operation result
     */
    SecurityResult secureMessage(const std::string& plain_payload, 
                                 SecuredMessage& secured_msg);
    
    /**
     * @brief Generate secured JSON envelope
     * @param secured_msg Secured message structure
     * @return JSON string with nonce, payload, and MAC
     */
    std::string generateSecuredEnvelope(const SecuredMessage& secured_msg);
    
    // ========== Incoming Message Verification ==========
    
    /**
     * @brief Verify and extract message
     * @param secured_json Secured JSON envelope
     * @param plain_payload Output plain payload
     * @return Security operation result
     */
    SecurityResult verifyMessage(const std::string& secured_json,
                                 std::string& plain_payload);
    
    /**
     * @brief Parse secured envelope
     * @param secured_json JSON string
     * @param secured_msg Output secured message structure
     * @return true if parsing successful
     */
    bool parseSecuredEnvelope(const std::string& secured_json,
                             SecuredMessage& secured_msg);
    
    // ========== Nonce Management ==========
    
    /**
     * @brief Get next nonce for outgoing message
     * @return New nonce value
     */
    uint32_t getNextNonce();
    
    /**
     * @brief Check if nonce is valid (anti-replay)
     * @param nonce Nonce to check
     * @return true if nonce is acceptable
     */
    bool isNonceValid(uint32_t nonce);
    
    /**
     * @brief Update last seen nonce
     * @param nonce Nonce value
     */
    void updateLastNonce(uint32_t nonce);
    
    /**
     * @brief Load nonce state from persistent storage
     * @return true if loaded successfully
     */
    bool loadNonceState();
    
    /**
     * @brief Save nonce state to persistent storage
     * @return true if saved successfully
     */
    bool saveNonceState();
    
    // ========== Cryptographic Operations ==========
    
    /**
     * @brief Compute HMAC-SHA256
     * @param data Input data
     * @param key HMAC key
     * @return HMAC tag (hex string)
     */
    std::string computeHMAC(const std::string& data, const std::string& key);
    
    /**
     * @brief Verify HMAC
     * @param data Input data
     * @param mac Expected MAC (hex string)
     * @param key HMAC key
     * @return true if MAC is valid
     */
    bool verifyHMAC(const std::string& data, const std::string& mac, 
                   const std::string& key);
    
    /**
     * @brief Encrypt data using AES-CBC
     * @param plain_data Plain data
     * @param encrypted_data Output encrypted data (base64)
     * @return true if encryption successful
     */
    bool encryptAES(const std::string& plain_data, std::string& encrypted_data);
    
    /**
     * @brief Decrypt data using AES-CBC
     * @param encrypted_data Encrypted data (base64)
     * @param plain_data Output plain data
     * @return true if decryption successful
     */
    bool decryptAES(const std::string& encrypted_data, std::string& plain_data);
    
    /**
     * @brief Simulate encryption with base64 encoding
     * @param plain_data Plain data
     * @return Base64 encoded data
     */
    std::string simulateEncryption(const std::string& plain_data);
    
    /**
     * @brief Simulate decryption with base64 decoding
     * @param encoded_data Base64 encoded data
     * @return Decoded data
     */
    std::string simulateDecryption(const std::string& encoded_data);
    
    // ========== Configuration ==========
    
    /**
     * @brief Update security configuration
     * @param config New configuration
     */
    void updateConfig(const SecurityConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Current security configuration
     */
    SecurityConfig getConfig() const;
    
    /**
     * @brief Update PSK
     * @param new_psk New pre-shared key (hex string)
     * @return true if update successful
     */
    bool updatePSK(const std::string& new_psk);
    
    // ========== Statistics & Diagnostics ==========
    
    /**
     * @brief Get security statistics
     * @return JSON string with stats
     */
    std::string getSecurityStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats();
    
private:
    SecurityConfig config_;
    
    // Nonce state
    uint32_t current_nonce_;           // Current outgoing nonce
    uint32_t last_received_nonce_;     // Last valid received nonce
    std::vector<uint32_t> recent_nonces_; // Recent nonces for replay detection
    
    // Statistics
    uint32_t messages_secured_;
    uint32_t messages_verified_;
    uint32_t replay_attempts_;
    uint32_t mac_failures_;
    
    // Internal helpers
    bool initializeCrypto();
    void cleanupCrypto();
    uint32_t estimateRecoveryNonce(); // Smart nonce recovery when state loading fails
    std::string bytesToHex(const uint8_t* bytes, size_t len) const;
    bool hexToBytes(const std::string& hex, uint8_t* bytes, size_t max_len) const;
    std::string base64Encode(const uint8_t* data, size_t len) const;
    bool base64Decode(const std::string& encoded, std::vector<uint8_t>& decoded) const;
    
    // AES-CBC implementation helpers
    bool aesEncryptCBC(const uint8_t* plain, size_t plain_len,
                      uint8_t* cipher, size_t& cipher_len,
                      const uint8_t* key, const uint8_t* iv);
    bool aesDecryptCBC(const uint8_t* cipher, size_t cipher_len,
                      uint8_t* plain, size_t& plain_len,
                      const uint8_t* key, const uint8_t* iv);
    
    // Thread safety (if needed)
    bool mutex_initialized_;
    void lock();
    void unlock();
    
    // Constants
    static constexpr size_t PSK_SIZE = 32;        // 256 bits
    static constexpr size_t HMAC_SIZE = 32;       // SHA256 output
    static constexpr size_t AES_KEY_SIZE = 32;    // AES-256
    static constexpr size_t AES_BLOCK_SIZE = 16;  // AES block size
    static constexpr size_t MAX_NONCE_HISTORY = 100;
    
    static constexpr const char* NONCE_FILE = "/security/nonce.dat";
};

/**
 * @brief Helper to convert security status to string
 */
inline const char* securityStatusToString(SecurityStatus status) {
    switch (status) {
        case SecurityStatus::SUCCESS: return "success";
        case SecurityStatus::INVALID_MAC: return "invalid_mac";
        case SecurityStatus::REPLAY_DETECTED: return "replay_detected";
        case SecurityStatus::NONCE_TOO_OLD: return "nonce_too_old";
        case SecurityStatus::ENCRYPTION_ERROR: return "encryption_error";
        case SecurityStatus::INVALID_FORMAT: return "invalid_format";
        case SecurityStatus::KEY_ERROR: return "key_error";
        default: return "unknown";
    }
}
