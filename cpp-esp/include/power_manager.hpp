#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <stdint.h>
#include <string>

/**
 * @file power_manager.hpp
 * @brief Power Management Module for EcoWatt Device (Milestone 5 Part 1)
 * 
 * Implements power-saving mechanisms including:
 * - Dynamic CPU frequency scaling (80MHz ↔ 160MHz ↔ 240MHz)
 * - WiFi light sleep mode
 * - Peripheral gating (WiFi, ADC power management)
 * - Power measurement and statistics
 * - Self-energy-use reporting
 * 
 * Based on ESP32 capabilities (since platformio.ini shows esp32dev board).
 * Note: Resource document mentions ESP8266, but your platformio.ini uses ESP32.
 */

// Power modes
enum class PowerMode {
    HIGH_PERFORMANCE,   // 240 MHz, WiFi always on, no sleep
    NORMAL,             // 160 MHz, WiFi light sleep, moderate power
    LOW_POWER,          // 80 MHz, WiFi light sleep, aggressive gating
    ULTRA_LOW_POWER     // 80 MHz, WiFi deep sleep between windows (not fully implemented)
};

// Power statistics
struct PowerStats {
    PowerMode current_mode;
    uint32_t cpu_freq_mhz;
    bool wifi_sleep_enabled;
    bool wifi_active;
    bool adc_active;
    uint32_t mode_switches;
    uint32_t total_sleep_ms;
    uint32_t total_active_ms;
    unsigned long uptime_ms;
    
    // Estimated power metrics (if no INA219 available)
    float estimated_current_ma;
    float estimated_power_mw;
    
    PowerStats() 
        : current_mode(PowerMode::NORMAL)
        , cpu_freq_mhz(160)
        , wifi_sleep_enabled(false)
        , wifi_active(true)
        , adc_active(false)
        , mode_switches(0)
        , total_sleep_ms(0)
        , total_active_ms(0)
        , uptime_ms(0)
        , estimated_current_ma(0.0f)
        , estimated_power_mw(0.0f) {}
};

// Power configuration
struct PowerConfig {
    bool enable_cpu_scaling;        // Enable dynamic CPU frequency scaling
    bool enable_wifi_sleep;         // Enable WiFi light sleep
    bool enable_peripheral_gating;  // Enable peripheral power gating
    bool enable_auto_mode;          // Automatically switch modes based on activity
    PowerMode default_mode;         // Default power mode
    uint32_t idle_timeout_ms;       // Time before entering low power mode
    bool enable_power_reporting;    // Enable self-energy reporting
    
    PowerConfig()
        : enable_cpu_scaling(true)
        , enable_wifi_sleep(true)
        , enable_peripheral_gating(true)
        , enable_auto_mode(true)
        , default_mode(PowerMode::NORMAL)
        , idle_timeout_ms(5000)
        , enable_power_reporting(true) {}
};

class PowerManager {
public:
    /**
     * @brief Constructor
     * @param config Power management configuration
     */
    explicit PowerManager(const PowerConfig& config);
    ~PowerManager();
    
    /**
     * @brief Initialize power manager
     * Must be called after WiFi initialization
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Shutdown power manager
     */
    void end();
    
    /**
     * @brief Main loop - call periodically
     * Handles automatic mode switching and statistics updates
     */
    void loop();
    
    // ========== Power Mode Management ==========
    
    /**
     * @brief Set power mode
     * @param mode Target power mode
     * @return true if mode change successful
     */
    bool setPowerMode(PowerMode mode);
    
    /**
     * @brief Get current power mode
     */
    PowerMode getPowerMode() const { return stats_.current_mode; }
    
    /**
     * @brief Signal activity to prevent automatic sleep
     * Call this when entering active periods (acquisition, upload, etc.)
     */
    void signalActivity();
    
    /**
     * @brief Signal idle state to allow automatic sleep
     * Call this when entering idle periods (between polls, after upload, etc.)
     */
    void signalIdle();
    
    // ========== CPU Frequency Scaling ==========
    
    /**
     * @brief Set CPU frequency
     * @param freq_mhz Frequency in MHz (80, 160, or 240)
     * @return true if frequency change successful
     */
    bool setCpuFrequency(uint32_t freq_mhz);
    
    /**
     * @brief Get current CPU frequency
     * @return Frequency in MHz
     */
    uint32_t getCpuFrequency() const;
    
    // ========== WiFi Power Management ==========
    
    /**
     * @brief Enable WiFi light sleep mode
     * WiFi will automatically sleep between packets
     * @return true if successful
     */
    bool enableWiFiSleep();
    
    /**
     * @brief Disable WiFi light sleep mode
     * @return true if successful
     */
    bool disableWiFiSleep();
    
    /**
     * @brief Temporarily wake WiFi for active communication
     * Use before HTTP requests or other network operations
     */
    void wakeWiFi();
    
    /**
     * @brief Allow WiFi to sleep after communication
     * Use after HTTP requests complete
     */
    void sleepWiFi();
    
    // ========== Peripheral Gating ==========
    
    /**
     * @brief Power down ADC
     */
    void powerDownADC();
    
    /**
     * @brief Power up ADC
     */
    void powerUpADC();
    
    // ========== Power Measurement & Statistics ==========
    
    /**
     * @brief Get power statistics
     */
    PowerStats getStats() const { return stats_; }
    
    /**
     * @brief Get power statistics as JSON string
     */
    std::string getStatsJson() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats();
    
    /**
     * @brief Update estimated power consumption
     * Based on current configuration and operating mode
     */
    void updatePowerEstimate();
    
    /**
     * @brief Get estimated current draw in mA
     */
    float getEstimatedCurrent() const { return stats_.estimated_current_ma; }
    
    /**
     * @brief Get estimated power consumption in mW
     */
    float getEstimatedPower() const { return stats_.estimated_power_mw; }
    
    /**
     * @brief Generate power comparison report
     * Compares baseline (no power management) vs current configuration
     * @return Markdown-formatted report
     */
    std::string generatePowerReport() const;
    
    // ========== Configuration ==========
    
    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void updateConfig(const PowerConfig& config);
    
    /**
     * @brief Get current configuration
     */
    PowerConfig getConfig() const { return config_; }

private:
    PowerConfig config_;
    PowerStats stats_;
    
    // Activity tracking
    unsigned long last_activity_ms_;
    bool is_active_;
    
    // Baseline power estimates (ESP32 typical values)
    static constexpr float BASELINE_CURRENT_240MHZ_WIFI_ACTIVE = 160.0f;  // mA
    static constexpr float BASELINE_CURRENT_160MHZ_WIFI_ACTIVE = 95.0f;   // mA
    static constexpr float BASELINE_CURRENT_80MHZ_WIFI_ACTIVE = 80.0f;    // mA
    static constexpr float BASELINE_CURRENT_240MHZ_WIFI_SLEEP = 30.0f;    // mA
    static constexpr float BASELINE_CURRENT_160MHZ_WIFI_SLEEP = 20.0f;    // mA
    static constexpr float BASELINE_CURRENT_80MHZ_WIFI_SLEEP = 15.0f;     // mA
    static constexpr float WIFI_ACTIVE_OVERHEAD = 70.0f;                  // mA additional when WiFi active
    static constexpr float ADC_POWER = 1.0f;                              // mA when ADC active
    static constexpr float SUPPLY_VOLTAGE = 3.3f;                         // V
    
    // Internal helpers
    bool applyPowerMode(PowerMode mode);
    void checkAutoModeSwitch();
    float estimateCurrentForMode(PowerMode mode) const;
};

/**
 * @brief Convert power mode to string
 */
inline const char* powerModeToString(PowerMode mode) {
    switch (mode) {
        case PowerMode::HIGH_PERFORMANCE: return "HIGH_PERFORMANCE";
        case PowerMode::NORMAL: return "NORMAL";
        case PowerMode::LOW_POWER: return "LOW_POWER";
        case PowerMode::ULTRA_LOW_POWER: return "ULTRA_LOW_POWER";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert string to power mode
 */
inline PowerMode stringToPowerMode(const char* str) {
    if (strcmp(str, "HIGH_PERFORMANCE") == 0) return PowerMode::HIGH_PERFORMANCE;
    if (strcmp(str, "NORMAL") == 0) return PowerMode::NORMAL;
    if (strcmp(str, "LOW_POWER") == 0) return PowerMode::LOW_POWER;
    if (strcmp(str, "ULTRA_LOW_POWER") == 0) return PowerMode::ULTRA_LOW_POWER;
    return PowerMode::NORMAL; // Default
}
