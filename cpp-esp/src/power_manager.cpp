#include "../include/power_manager.hpp"
#include "../include/logger.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_pm.h>
#include <driver/adc.h>
#include <soc/rtc.h>

PowerManager::PowerManager(const PowerConfig& config)
    : config_(config)
    , last_activity_ms_(0)
    , is_active_(true) {
    // Initialize stats
    stats_.current_mode = config.default_mode;
    stats_.cpu_freq_mhz = getCpuFrequency();
    stats_.uptime_ms = millis();
}

PowerManager::~PowerManager() {
    end();
}

bool PowerManager::begin() {
    Logger::info("[PowerMgr] Initializing Power Manager...");
    
    // Apply default power mode
    if (!applyPowerMode(config_.default_mode)) {
        Logger::error("[PowerMgr] Failed to apply default power mode");
        return false;
    }
    
    // Initialize activity tracking
    last_activity_ms_ = millis();
    is_active_ = true;
    
    // Update initial power estimate
    updatePowerEstimate();
    
    Logger::info("[PowerMgr] Power Manager initialized");
    Logger::info("[PowerMgr]   CPU Frequency: %u MHz", stats_.cpu_freq_mhz);
    Logger::info("[PowerMgr]   Power Mode: %s", powerModeToString(stats_.current_mode));
    Logger::info("[PowerMgr]   CPU Scaling: %s", config_.enable_cpu_scaling ? "ENABLED" : "DISABLED");
    Logger::info("[PowerMgr]   WiFi Sleep: %s", config_.enable_wifi_sleep ? "ENABLED" : "DISABLED");
    Logger::info("[PowerMgr]   Peripheral Gating: %s", config_.enable_peripheral_gating ? "ENABLED" : "DISABLED");
    Logger::info("[PowerMgr]   Estimated Current: %.2f mA", stats_.estimated_current_ma);
    Logger::info("[PowerMgr]   Estimated Power: %.2f mW", stats_.estimated_power_mw);
    
    return true;
}

void PowerManager::end() {
    Logger::info("[PowerMgr] Shutting down Power Manager");
    
    // Restore high performance mode before shutdown
    setPowerMode(PowerMode::HIGH_PERFORMANCE);
}

void PowerManager::loop() {
    // Update uptime
    stats_.uptime_ms = millis();
    
    // Check for automatic mode switching based on activity
    if (config_.enable_auto_mode) {
        checkAutoModeSwitch();
    }
    
    // Update power estimates periodically (every 10 seconds)
    static unsigned long last_estimate_update = 0;
    if (millis() - last_estimate_update > 10000) {
        updatePowerEstimate();
        last_estimate_update = millis();
    }
}

bool PowerManager::setPowerMode(PowerMode mode) {
    if (stats_.current_mode == mode) {
        return true; // Already in this mode
    }
    
    Logger::info("[PowerMgr] Switching power mode: %s -> %s",
                 powerModeToString(stats_.current_mode),
                 powerModeToString(mode));
    
    bool success = applyPowerMode(mode);
    if (success) {
        stats_.current_mode = mode;
        stats_.mode_switches++;
        updatePowerEstimate();
        
        Logger::info("[PowerMgr] Power mode switched successfully");
        Logger::info("[PowerMgr]   New Current Estimate: %.2f mA", stats_.estimated_current_ma);
    } else {
        Logger::error("[PowerMgr] Failed to switch power mode");
    }
    
    return success;
}

void PowerManager::signalActivity() {
    last_activity_ms_ = millis();
    is_active_ = true;
    
    // If in low power mode, switch to normal for activity
    if (config_.enable_auto_mode && stats_.current_mode == PowerMode::LOW_POWER) {
        setPowerMode(PowerMode::NORMAL);
    }
}

void PowerManager::signalIdle() {
    is_active_ = false;
    last_activity_ms_ = millis();
}

bool PowerManager::setCpuFrequency(uint32_t freq_mhz) {
    if (!config_.enable_cpu_scaling) {
        Logger::warn("[PowerMgr] CPU scaling is disabled in config");
        return false;
    }
    
    // ESP32 supports 80, 160, 240 MHz
    if (freq_mhz != 80 && freq_mhz != 160 && freq_mhz != 240) {
        Logger::error("[PowerMgr] Invalid CPU frequency: %u MHz (supported: 80, 160, 240)", freq_mhz);
        return false;
    }
    
    Logger::info("[PowerMgr] Setting CPU frequency to %u MHz", freq_mhz);
    
    bool success = setCpuFrequencyMhz(freq_mhz);
    if (success) {
        stats_.cpu_freq_mhz = getCpuFrequency();
        Logger::info("[PowerMgr] CPU frequency set to %u MHz", stats_.cpu_freq_mhz);
    } else {
        Logger::error("[PowerMgr] Failed to set CPU frequency");
    }
    
    return success;
}

uint32_t PowerManager::getCpuFrequency() const {
    return getCpuFrequencyMhz();
}

bool PowerManager::enableWiFiSleep() {
    if (!config_.enable_wifi_sleep) {
        Logger::warn("[PowerMgr] WiFi sleep is disabled in config");
        return false;
    }
    
    Logger::info("[PowerMgr] Enabling WiFi light sleep");
    
    // Enable WiFi power save mode (light sleep)
    esp_err_t err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (err == ESP_OK) {
        stats_.wifi_sleep_enabled = true;
        Logger::info("[PowerMgr] WiFi light sleep enabled");
        return true;
    } else {
        Logger::error("[PowerMgr] Failed to enable WiFi light sleep: %d", err);
        return false;
    }
}

bool PowerManager::disableWiFiSleep() {
    Logger::info("[PowerMgr] Disabling WiFi sleep");
    
    esp_err_t err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err == ESP_OK) {
        stats_.wifi_sleep_enabled = false;
        Logger::info("[PowerMgr] WiFi sleep disabled");
        return true;
    } else {
        Logger::error("[PowerMgr] Failed to disable WiFi sleep: %d", err);
        return false;
    }
}

void PowerManager::wakeWiFi() {
    if (stats_.wifi_sleep_enabled) {
        // WiFi will wake automatically on network activity
        // We just mark it as active for statistics
        stats_.wifi_active = true;
    }
}

void PowerManager::sleepWiFi() {
    if (stats_.wifi_sleep_enabled) {
        stats_.wifi_active = false;
        // WiFi will sleep automatically when idle
    }
}

void PowerManager::powerDownADC() {
    if (!config_.enable_peripheral_gating) {
        return;
    }
    
    Logger::debug("[PowerMgr] Powering down ADC");
    
    // Disable ADC
    adc_power_off();
    stats_.adc_active = false;
}

void PowerManager::powerUpADC() {
    if (!config_.enable_peripheral_gating) {
        return;
    }
    
    Logger::debug("[PowerMgr] Powering up ADC");
    
    // Enable ADC
    adc_power_on();
    stats_.adc_active = true;
}

std::string PowerManager::getStatsJson() const {
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{"
             "\"mode\":\"%s\","
             "\"cpu_freq_mhz\":%u,"
             "\"wifi_sleep\":%s,"
             "\"wifi_active\":%s,"
             "\"adc_active\":%s,"
             "\"mode_switches\":%u,"
             "\"uptime_ms\":%lu,"
             "\"estimated_current_ma\":%.2f,"
             "\"estimated_power_mw\":%.2f"
             "}",
             powerModeToString(stats_.current_mode),
             stats_.cpu_freq_mhz,
             stats_.wifi_sleep_enabled ? "true" : "false",
             stats_.wifi_active ? "true" : "false",
             stats_.adc_active ? "true" : "false",
             stats_.mode_switches,
             stats_.uptime_ms,
             stats_.estimated_current_ma,
             stats_.estimated_power_mw);
    return std::string(buf);
}

void PowerManager::resetStats() {
    Logger::info("[PowerMgr] Resetting statistics");
    
    stats_.mode_switches = 0;
    stats_.total_sleep_ms = 0;
    stats_.total_active_ms = 0;
    stats_.uptime_ms = millis();
}

void PowerManager::updatePowerEstimate() {
    stats_.estimated_current_ma = estimateCurrentForMode(stats_.current_mode);
    stats_.estimated_power_mw = stats_.estimated_current_ma * SUPPLY_VOLTAGE;
}

std::string PowerManager::generatePowerReport() const {
    // Calculate baseline power (HIGH_PERFORMANCE mode, no optimizations)
    float baseline_current = BASELINE_CURRENT_240MHZ_WIFI_ACTIVE + ADC_POWER;
    float baseline_power = baseline_current * SUPPLY_VOLTAGE;
    
    // Calculate current optimized power
    float optimized_current = stats_.estimated_current_ma;
    float optimized_power = stats_.estimated_power_mw;
    
    // Calculate savings
    float current_savings_ma = baseline_current - optimized_current;
    float current_savings_percent = (current_savings_ma / baseline_current) * 100.0f;
    float power_savings_mw = baseline_power - optimized_power;
    float power_savings_percent = (power_savings_mw / baseline_power) * 100.0f;
    
    char report[2048];
    snprintf(report, sizeof(report),
             "# Power Management Report - EcoWatt Device\n"
             "\n"
             "## Test Configuration\n"
             "- **Device**: ESP32 (NodeMCU)\n"
             "- **Test Duration**: %lu seconds\n"
             "- **Power Optimizations Applied**:\n"
             "  - CPU Frequency Scaling: %s\n"
             "  - WiFi Light Sleep: %s\n"
             "  - Peripheral Gating: %s\n"
             "  - Automatic Mode Switching: %s\n"
             "\n"
             "## Current Configuration\n"
             "- **Power Mode**: %s\n"
             "- **CPU Frequency**: %u MHz\n"
             "- **WiFi Sleep Mode**: %s\n"
             "- **WiFi Currently Active**: %s\n"
             "- **ADC Active**: %s\n"
             "- **Mode Switches**: %u\n"
             "\n"
             "## Power Measurements\n"
             "\n"
             "### Baseline (No Power Management)\n"
             "- **Mode**: HIGH_PERFORMANCE (240 MHz, WiFi always on)\n"
             "- **Estimated Current**: %.2f mA\n"
             "- **Estimated Power**: %.2f mW\n"
             "\n"
             "### Optimized (Current Configuration)\n"
             "- **Mode**: %s\n"
             "- **Estimated Current**: %.2f mA\n"
             "- **Estimated Power**: %.2f mW\n"
             "\n"
             "### Power Savings\n"
             "- **Current Reduction**: %.2f mA (%.1f%%)\n"
             "- **Power Reduction**: %.2f mW (%.1f%%)\n"
             "\n"
             "## Methodology\n"
             "\n"
             "Power measurements are **estimated** based on ESP32 datasheet typical values:\n"
             "\n"
             "| Configuration | CPU (MHz) | WiFi State | Typical Current (mA) |\n"
             "|--------------|-----------|------------|---------------------|\n"
             "| High Performance | 240 | Active | 160 |\n"
             "| Normal | 160 | Active | 95 |\n"
             "| Low Power | 80 | Active | 80 |\n"
             "| High Performance | 240 | Light Sleep | 30 |\n"
             "| Normal | 160 | Light Sleep | 20 |\n"
             "| Low Power | 80 | Light Sleep | 15 |\n"
             "\n"
             "**Note**: Actual power consumption may vary based on workload, peripheral usage,\n"
             "and environmental conditions. For precise measurements, use a USB power meter\n"
             "or INA219 sensor as described in Milestone 5 resources.\n"
             "\n"
             "## Justification of Choices\n"
             "\n"
             "1. **CPU Frequency Scaling**: %s\n"
             "   - During idle periods (between polls), CPU runs at 80 MHz\n"
             "   - During active operations (acquisition, upload), CPU runs at 160-240 MHz\n"
             "   - Provides significant power savings with minimal performance impact\n"
             "\n"
             "2. **WiFi Light Sleep**: %s\n"
             "   - WiFi automatically sleeps between packet transmissions\n"
             "   - Wakes automatically for incoming/outgoing traffic\n"
             "   - Reduces power consumption by ~70%% during idle periods\n"
             "   - Compatible with HTTP request/response operations\n"
             "\n"
             "3. **Peripheral Gating**: %s\n"
             "   - ADC powered down when not in use\n"
             "   - Minimal impact (~1 mA savings) but implemented for completeness\n"
             "\n"
             "4. **Automatic Mode Switching**: %s\n"
             "   - System automatically switches to LOW_POWER mode after %u ms of inactivity\n"
             "   - Switches to NORMAL mode when activity detected\n"
             "   - Balances power savings with responsiveness\n"
             "\n"
             "## Conclusion\n"
             "\n"
             "Power management reduces estimated current consumption by **%.1f%%** (%.2f mA),\n"
             "resulting in **%.1f%%** power savings (%.2f mW). This demonstrates significant\n"
             "energy efficiency improvements while maintaining full functionality.\n",
             stats_.uptime_ms / 1000,
             config_.enable_cpu_scaling ? "ENABLED" : "DISABLED",
             config_.enable_wifi_sleep ? "ENABLED" : "DISABLED",
             config_.enable_peripheral_gating ? "ENABLED" : "DISABLED",
             config_.enable_auto_mode ? "ENABLED" : "DISABLED",
             powerModeToString(stats_.current_mode),
             stats_.cpu_freq_mhz,
             stats_.wifi_sleep_enabled ? "ENABLED" : "DISABLED",
             stats_.wifi_active ? "YES" : "NO",
             stats_.adc_active ? "YES" : "NO",
             stats_.mode_switches,
             baseline_current,
             baseline_power,
             powerModeToString(stats_.current_mode),
             optimized_current,
             optimized_power,
             current_savings_ma,
             current_savings_percent,
             power_savings_mw,
             power_savings_percent,
             config_.enable_cpu_scaling ? "Reduces current by up to 50 mA" : "Not implemented",
             config_.enable_wifi_sleep ? "Reduces current by ~70 mA during idle" : "Not implemented",
             config_.enable_peripheral_gating ? "Reduces current by ~1 mA" : "Not implemented",
             config_.enable_auto_mode ? "Optimizes power based on activity" : "Not implemented",
             config_.idle_timeout_ms,
             current_savings_percent,
             current_savings_ma,
             power_savings_percent,
             power_savings_mw);
    
    return std::string(report);
}

void PowerManager::updateConfig(const PowerConfig& config) {
    Logger::info("[PowerMgr] Updating configuration");
    
    config_ = config;
    
    // Re-apply current power mode with new configuration
    applyPowerMode(stats_.current_mode);
}

// ========== Private Methods ==========

bool PowerManager::applyPowerMode(PowerMode mode) {
    bool success = true;
    
    switch (mode) {
        case PowerMode::HIGH_PERFORMANCE:
            // 240 MHz, WiFi always on, no sleep
            if (config_.enable_cpu_scaling) {
                success &= setCpuFrequency(240);
            }
            success &= disableWiFiSleep();
            stats_.wifi_active = true;
            break;
            
        case PowerMode::NORMAL:
            // 160 MHz, WiFi light sleep
            if (config_.enable_cpu_scaling) {
                success &= setCpuFrequency(160);
            }
            if (config_.enable_wifi_sleep) {
                success &= enableWiFiSleep();
            }
            break;
            
        case PowerMode::LOW_POWER:
            // 80 MHz, WiFi light sleep, aggressive gating
            if (config_.enable_cpu_scaling) {
                success &= setCpuFrequency(80);
            }
            if (config_.enable_wifi_sleep) {
                success &= enableWiFiSleep();
            }
            if (config_.enable_peripheral_gating) {
                powerDownADC();
            }
            break;
            
        case PowerMode::ULTRA_LOW_POWER:
            // 80 MHz, WiFi deep sleep (not fully implemented)
            if (config_.enable_cpu_scaling) {
                success &= setCpuFrequency(80);
            }
            if (config_.enable_wifi_sleep) {
                success &= enableWiFiSleep();
            }
            if (config_.enable_peripheral_gating) {
                powerDownADC();
            }
            Logger::warn("[PowerMgr] ULTRA_LOW_POWER mode not fully implemented");
            break;
    }
    
    return success;
}

void PowerManager::checkAutoModeSwitch() {
    unsigned long idle_time = millis() - last_activity_ms_;
    
    // If idle for longer than timeout, switch to low power
    if (!is_active_ && idle_time > config_.idle_timeout_ms) {
        if (stats_.current_mode != PowerMode::LOW_POWER) {
            Logger::info("[PowerMgr] Idle timeout reached, switching to LOW_POWER mode");
            setPowerMode(PowerMode::LOW_POWER);
        }
    }
    // If active, ensure we're in normal or high performance mode
    else if (is_active_ && stats_.current_mode == PowerMode::LOW_POWER) {
        Logger::info("[PowerMgr] Activity detected, switching to NORMAL mode");
        setPowerMode(PowerMode::NORMAL);
    }
}

float PowerManager::estimateCurrentForMode(PowerMode mode) const {
    float base_current = 0.0f;
    
    // Base current depends on CPU frequency and WiFi state
    switch (mode) {
        case PowerMode::HIGH_PERFORMANCE:
            base_current = BASELINE_CURRENT_240MHZ_WIFI_ACTIVE;
            break;
            
        case PowerMode::NORMAL:
            if (stats_.wifi_sleep_enabled && !stats_.wifi_active) {
                base_current = BASELINE_CURRENT_160MHZ_WIFI_SLEEP;
            } else {
                base_current = BASELINE_CURRENT_160MHZ_WIFI_ACTIVE;
            }
            break;
            
        case PowerMode::LOW_POWER:
            if (stats_.wifi_sleep_enabled && !stats_.wifi_active) {
                base_current = BASELINE_CURRENT_80MHZ_WIFI_SLEEP;
            } else {
                base_current = BASELINE_CURRENT_80MHZ_WIFI_ACTIVE;
            }
            break;
            
        case PowerMode::ULTRA_LOW_POWER:
            // Similar to LOW_POWER for now
            base_current = BASELINE_CURRENT_80MHZ_WIFI_SLEEP;
            break;
    }
    
    // Add ADC overhead if active
    if (stats_.adc_active) {
        base_current += ADC_POWER;
    }
    
    return base_current;
}
