#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <functional>
#include "ticker_fallback.hpp"
#include "../include/ecoWatt_device.hpp"
#include "../include/acquisition_scheduler.hpp"
#include "../include/data_storage.hpp"
#include "../include/protocol_adapter.hpp"
#include "../include/config_manager.hpp"
#include "../include/uplink_packetizer.hpp"
#include "../include/remote_config_handler.hpp"
#include "../include/command_executor.hpp"
#include "../include/http_client.hpp"
#include "../include/logger.hpp"
#include "../include/wifi_connector.hpp"
// --- Heap/Stack debug print helper ---
#ifdef ESP32
#include <Arduino.h>
#endif
#include <Arduino.h>
static void printMemoryStats(const char* tag) {
    // Temporarily disabled to prevent stack overflow during demo
    // #ifdef ESP32
    //     Logger::info("[MEM] %s | Free heap: %u bytes | Min heap: %u bytes", tag, ESP.getFreeHeap(), ESP.getMinFreeHeap());
    // #endif
    //     char dummy;
    //     Logger::info("[MEM] %s | Stack ptr: %p", tag, &dummy);
}
EcoWattDevice::EcoWattDevice() {}
EcoWattDevice::~EcoWattDevice() {
    delete scheduler_;
    delete adapter_;
    delete storage_;
    delete uplink_packetizer_;
    delete remote_config_handler_;
    delete command_executor_;
    delete fota_;
    delete secure_http_;
    delete security_;
    delete http_client_;
    delete config_;
    delete wifi_;
    delete power_mgr_;
    delete fault_handler_;
    delete event_logger_;
}

// Simple JSON helpers (very small, only for our known command shapes)
static bool extractJsonStringFieldLocal(const char* json, const char* key, char* outBuf, size_t outBufSize) {
    if (!json || !key || !outBuf) return false;
    char pat[128];
    size_t klen = strlen(key);
    if (klen + 3 >= sizeof(pat)) return false;
    pat[0] = '"';
    memcpy(pat + 1, key, klen);
    pat[1 + klen] = '"';
    pat[2 + klen] = '\0';
    const char* found = strstr(json, pat);
    if (!found) return false;
    const char* colon = strchr(found + strlen(pat), ':');
    if (!colon) return false;
    const char* q1 = strchr(colon, '"');
    if (!q1) return false;
    const char* q2 = strchr(q1 + 1, '"');
    if (!q2) return false;
    size_t len = q2 - (q1 + 1);
    if (len + 1 > outBufSize) return false;
    memcpy(outBuf, q1 + 1, len);
    outBuf[len] = '\0';
    return true;
}

static bool extractJsonNumberFieldLocal(const char* json, const char* key, char* outNumBuf, size_t outNumBufSize) {
    if (!json || !key || !outNumBuf) return false;
    char pat[128];
    size_t klen = strlen(key);
    if (klen + 2 >= sizeof(pat)) return false;
    pat[0] = '"';
    memcpy(pat + 1, key, klen);
    pat[1 + klen] = '"';
    pat[2 + klen] = '\0';
    const char* found = strstr(json, pat);
    if (!found) return false;
    const char* colon = strchr(found + strlen(pat), ':');
    if (!colon) return false;
    // Skip spaces
    const char* p = colon + 1;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    // Read until non-number (allow digits, minus, dot, exponent, etc.)
    const char* start = p;
    const char* end = start;
    while (*end && ( (*end >= '0' && *end <= '9') || *end == '-' || *end == '+' || *end == '.' || *end == 'e' || *end == 'E')) end++;
    size_t len = end - start;
    if (len == 0 || len + 1 > outNumBufSize) return false;
    memcpy(outNumBuf, start, len);
    outNumBuf[len] = '\0';
    return true;
}

bool EcoWattDevice::isOnline() const {
    return WiFi.status() == WL_CONNECTED;
}

float EcoWattDevice::getReading(uint8_t reg_addr) const {
    if (!adapter_ || !config_) return 0.0f;
    uint16_t raw_value = 0;
    if (adapter_->readRegisters(reg_addr, 1, &raw_value)) {
        RegisterConfig reg_config = config_->getRegisterConfig(reg_addr);
        if (reg_config.gain > 0) {
            return (float)raw_value / reg_config.gain;
        }
        return (float)raw_value;
    }
    return 0.0f; // Or NAN
}

bool EcoWattDevice::setControl(uint8_t reg_addr, float value) {
    if (!adapter_ || !config_) return false;

    RegisterConfig reg_config = config_->getRegisterConfig(reg_addr);
    if (reg_config.access.find("Write") == std::string::npos) {
        return false; // Not a writable register
    }

    uint16_t raw_value = (uint16_t)(value * reg_config.gain);
    return adapter_->writeRegister(reg_addr, raw_value);
}

void EcoWattDevice::getStatistics(char* outBuf, size_t outBufSize) const {
    // This can be expanded to gather more detailed stats from other modules
    // For example: scheduler_->getStatistics(buf, size);
    // For now, we provide basic uptime and connectivity.
    snprintf(outBuf, outBufSize, "uptime=%lu, online=%d", millis(), isOnline());
}

void EcoWattDevice::onConfigUpdated() {
    Logger::info("[EcoWattDevice] Remote configuration updated. Applying changes...");
    
    // Get the updated configuration
    AcquisitionConfig acq_conf = config_->getAcquisitionConfig();
    
    // Apply to acquisition scheduler in a thread-safe manner
    // The scheduler's updateConfig method will handle the update without interrupting ongoing acquisition
    if (scheduler_) {
        scheduler_->updateConfig(
            acq_conf.minimum_registers.data(), 
            acq_conf.minimum_registers.size(), 
            acq_conf.polling_interval_ms
        );
        Logger::info("[EcoWattDevice] Applied config to scheduler: interval=%u ms, registers=%u",
                     acq_conf.polling_interval_ms,
                     (unsigned)acq_conf.minimum_registers.size());
    } else {
        Logger::warn("[EcoWattDevice] Scheduler not initialized, cannot apply config");
    }
}

void EcoWattDevice::onCommandReceived(const CommandRequest& command) {
    Logger::info("[EcoWattDevice] Command received: id=%u, action=%s, target=%s, value=%.2f",
                 command.command_id, command.action.c_str(), 
                 command.target_register.c_str(), command.value);
    
    // Command is already queued by RemoteConfigHandler/CommandExecutor
    // This callback is just for logging/monitoring
}

void EcoWattDevice::setup() {
    Logger::info("EcoWatt Device initializing...");
    
    // Initialize WiFi, SPIFFS, config, etc.
    if (!config_) {
        config_ = new ConfigManager();
        Logger::info("ConfigManager initialized");
    }
    if (!storage_) {
        storage_ = new DataStorage();
        Logger::info("DataStorage initialized");
    }

    // Initialize Security Layer (if enabled in config)
    // Note: SecurityConfig needs to be retrieved from config
    // For now, we'll initialize it after WiFi is connected
    
    ApiConfig api_conf = config_->getApiConfig();
    ModbusConfig mbc = config_->getModbusConfig();
    if (!http_client_) {
        // This client is for INVERTER communication (Modbus proxy at 20.15.114.131)
        http_client_ = new EcoHttpClient(api_conf.inverter_base_url, mbc.timeout_ms);
        Logger::info("HTTP Client initialized with INVERTER base URL: %s", api_conf.inverter_base_url.c_str());
    }

    // WiFi: use wifi config if present in config.json, otherwise rely on /config/.env overrides
    if (!wifi_) {
        wifi_ = new WiFiConnector();
        Logger::info("WiFi Connector initialized");
    }
    // Hardcoded WiFi credentials
    wifi_->begin();
    
    // Wait for WiFi connection
    Logger::info("Waiting for WiFi connection...");
    unsigned long wifiStart = millis();
    while (!wifi_->isConnected() && millis() - wifiStart < 30000) { // 30 second timeout
        wifi_->loop();
        delay(500);
    }
    if (wifi_->isConnected()) {
        Logger::info("WiFi connected successfully");
    } else {
        Logger::error("WiFi connection failed after 30 seconds");
    }
    
    // ========== MILESTONE 5: Initialize Power Manager ==========
    if (!power_mgr_) {
        Logger::info("Initializing Power Manager...");
        
        PowerConfig power_config;
        power_config.enable_cpu_scaling = true;
        power_config.enable_wifi_sleep = true;
        power_config.enable_peripheral_gating = true;
        power_config.enable_auto_mode = true;
        power_config.default_mode = PowerMode::NORMAL;
        power_config.idle_timeout_ms = 5000;  // 5 seconds idle before low power
        power_config.enable_power_reporting = true;
        
        power_mgr_ = new PowerManager(power_config);
        if (power_mgr_->begin()) {
            Logger::info("Power Manager initialized successfully");
            
            // Generate and log initial power report
            std::string report = power_mgr_->generatePowerReport();
            Logger::info("[PowerMgr] Initial Power Report:\n%s", report.c_str());
        } else {
            Logger::error("Failed to initialize Power Manager");
        }
    }
    
    // Initialize Security Layer (after WiFi, before HTTP operations)
    if (!security_) {
        // Load security config from config.json
        // For Milestone 4, we'll use hardcoded PSK from config
        Logger::info("Initializing Security Layer...");
        
        // Create security config with proper initialization
        SecurityConfig sec_config = {
            .psk = "c41716a134168f52fbd4be3302fa5a88127ddde749501a199607b4c286ad29b3",
            .encryption_enabled = true,
            .use_real_encryption = false,
            .nonce_window = 100,
            .strict_nonce_checking = true
        };
        
        security_ = new SecurityLayer(sec_config);
        if (security_->begin()) {
            Logger::info("Security Layer initialized successfully");
            
            // Create SEPARATE HTTP client for CLOUD operations (config/upload at 10.52.180.183)
            EcoHttpClient* cloud_http_client = new EcoHttpClient(api_conf.upload_base_url, mbc.timeout_ms);
            Logger::info("Cloud HTTP Client initialized with base URL: %s", api_conf.upload_base_url.c_str());
            
            // Set API key and Device-ID for cloud client
            std::string device_id = config_->getDeviceId();
            const char* header_keys[] = {"Authorization", "Device-ID"};
            const char* header_values[] = {api_conf.api_key.c_str(), device_id.c_str()};
            cloud_http_client->setDefaultHeaders(header_keys, header_values, 2);
            
            // Create secure HTTP client wrapper using CLOUD client
            if (!secure_http_) {
                secure_http_ = new SecureHttpClient(cloud_http_client, security_);
                Logger::info("Secure HTTP Client initialized for CLOUD operations");
            }
        } else {
            Logger::error("Failed to initialize Security Layer");
        }
    }
    
    // Set the mandatory API key and Device-ID for all requests
    std::string device_id = config_->getDeviceId();
    const char* header_keys[] = {"Authorization", "Device-ID"};
    const char* header_values[] = {api_conf.api_key.c_str(), device_id.c_str()};
    http_client_->setDefaultHeaders(header_keys, header_values, 2);
    Logger::info("API key and Device-ID (%s) configured for requests", device_id.c_str());

    if (!adapter_) {
        adapter_ = new ProtocolAdapter(config_, http_client_);
        Logger::info("ProtocolAdapter initialized with slave address %d", mbc.slave_address);
    }

    // TEMPORARILY DISABLED FOR FOTA TESTING
    /*
    if (!uplink_packetizer_) {
        uplink_packetizer_ = new UplinkPacketizer(storage_, secure_http_);
        // Use the upload endpoint directly (should be a full URL)
        uplink_packetizer_->setCloudEndpoint(api_conf.upload_endpoint);
        uplink_packetizer_->begin(15 * 1000); // Upload every 15 seconds for demo
        Logger::info("UplinkPacketizer initialized with security enabled, upload interval: 15 seconds");
    }
    */
    Logger::info("UplinkPacketizer TEMPORARILY DISABLED for FOTA testing");

    if (!scheduler_) {
        scheduler_ = new AcquisitionScheduler(adapter_, storage_, config_);
        AcquisitionConfig acq_conf = config_->getAcquisitionConfig();
        scheduler_->updateConfig(acq_conf.minimum_registers.data(), acq_conf.minimum_registers.size(), acq_conf.polling_interval_ms);
        scheduler_->begin(acq_conf.polling_interval_ms);
        Logger::info("AcquisitionScheduler initialized with polling interval: %d ms", acq_conf.polling_interval_ms);
        
        // Signal activity to power manager when acquisition starts
        if (power_mgr_) {
            power_mgr_->signalActivity();
        }
    }

    if (!command_executor_) {
        command_executor_ = new CommandExecutor(adapter_, config_, http_client_);
        Logger::info("CommandExecutor initialized");
    }

    // RemoteConfigHandler RE-ENABLED after increasing loop stack to 16KB
    if (!remote_config_handler_) {
        remote_config_handler_ = new RemoteConfigHandler(config_, secure_http_, command_executor_);
        using namespace std::placeholders;
        remote_config_handler_->onConfigUpdate([this]() { onConfigUpdated(); });
        remote_config_handler_->onCommand([this](const CommandRequest& cmd) { onCommandReceived(cmd); });
        remote_config_handler_->begin(60000); // Check for new config every 60 seconds
        Logger::info("RemoteConfigHandler initialized with security enabled, check interval: 60 seconds");
    }

    // Initialize FOTA Manager (after WiFi and Security)
    if (!fota_) {
        Logger::info("Initializing FOTA Manager...");
        
        // LittleFS is already initialized in main.ino setup()
        // No need to re-initialize here
        
        // TEMPORARY: Clear boot count and FOTA state for fresh testing
        if (LittleFS.exists("/boot_count.txt")) {
            LittleFS.remove("/boot_count.txt");
            Logger::info("[FOTA TEST] Cleared boot_count.txt for fresh test");
        }
        if (LittleFS.exists("/fota_state.json")) {
            LittleFS.remove("/fota_state.json");
            Logger::info("[FOTA TEST] Cleared fota_state.json for fresh test");
        }
        
        // FOTA uses cloud HTTP client - FOTA endpoints are on cloud server (10.52.180.183)
        EcoHttpClient* cloud_http_for_fota = new EcoHttpClient(api_conf.upload_base_url, mbc.timeout_ms);
        fota_ = new FOTAManager(cloud_http_for_fota, security_, api_conf.upload_base_url);
        if (fota_->begin()) {
            Logger::info("FOTA Manager initialized successfully");

            // Report boot status to cloud
            fota_->reportBootStatus();

            // Check for firmware updates and start download immediately
            if (fota_->checkForUpdate()) {
                Logger::info("Firmware update available, starting download now");
                // Start the download; chunk processing runs in fota_->loop()
                if (!fota_->startDownload()) {
                    Logger::error("FOTA Manager failed to start download");
                }
            }
        } else {
            Logger::error("Failed to initialize FOTA Manager");
        }
    }

    // Initialize Event Logger for fault tracking
    Logger::info("Initializing Event Logger...");
    event_logger_ = new EventLogger();
    if (event_logger_->begin("/event_log.json", 100)) {
        Logger::info("Event Logger initialized successfully");
        event_logger_->logInfo("System boot", EventModule::SYSTEM, "EcoWatt Device starting up");
    } else {
        Logger::error("Failed to initialize Event Logger");
    }

    // Initialize Fault Handler
    Logger::info("Initializing Fault Handler...");
    fault_handler_ = new FaultHandler();
    if (fault_handler_->begin(event_logger_)) {
        Logger::info("Fault Handler initialized successfully");
    } else {
        Logger::error("Failed to initialize Fault Handler");
    }

    // Perform a one-time write operation as part of Milestone 2 requirements.
    // Write a safe default to a writable register if configured.
    // If register 8 is present and writable, write zero (as a demo)
    RegisterConfig rc = config_->getRegisterConfig(8);
    if (rc.access.find("Write") != std::string::npos) {
        uint16_t raw_value = (uint16_t)(0 * rc.gain);
        bool ok = adapter_->writeRegister(8, raw_value);
        Logger::info("Demo write to reg 8 result: %d", ok);
    }
    
    Logger::info("EcoWatt Device initialized successfully");
}

void EcoWattDevice::loop() {
    // Main polling, control, and data acquisition loop
    printMemoryStats("MainLoop");
    
    // ========== MILESTONE 5: Power Manager Loop ==========
    if (power_mgr_) {
        power_mgr_->loop();
    }
    
    // Storage operations
    if (storage_) storage_->loop();
    
    // Acquisition operations (active period)
    if (scheduler_) {
        if (power_mgr_) power_mgr_->signalActivity();
        scheduler_->loop();
    }
    
    // TEMPORARILY DISABLED FOR FOTA TESTING
    // if (uplink_packetizer_) uplink_packetizer_->loop();
    
    // Remote configuration and command handling
    if (remote_config_handler_) {
        // Wake WiFi before network operations
        if (power_mgr_) power_mgr_->wakeWiFi();
        
        remote_config_handler_->loop();
        remote_config_handler_->checkForCommands();
        
        // Allow WiFi to sleep after operations
        if (power_mgr_) power_mgr_->sleepWiFi();
    }
    
    // FOTA operations
    if (fota_) {
        // Wake WiFi before FOTA operations
        if (power_mgr_) power_mgr_->wakeWiFi();
        
        fota_->loop(); // Process FOTA chunk downloads
        
        // Allow WiFi to sleep after operations
        if (power_mgr_) power_mgr_->sleepWiFi();
    }
    
    // WiFi maintenance
    if (wifi_) wifi_->loop();
    
    // Signal idle to power manager after all operations
    static unsigned long last_activity_check = 0;
    if (millis() - last_activity_check > 1000) { // Check every second
        if (power_mgr_) {
            power_mgr_->signalIdle();
        }
        last_activity_check = millis();
    }
    
    // Log power statistics every 30 seconds
    static unsigned long last_power_log = 0;
    if (millis() - last_power_log > 30000) {
        if (power_mgr_) {
            PowerStats stats = power_mgr_->getStats();
            Logger::info("[PowerMgr] Stats: Mode=%s, CPU=%uMHz, WiFi_Sleep=%s, Current=%.2fmA, Power=%.2fmW",
                        powerModeToString(stats.current_mode),
                        stats.cpu_freq_mhz,
                        stats.wifi_sleep_enabled ? "ON" : "OFF",
                        stats.estimated_current_ma,
                        stats.estimated_power_mw);
        }
        last_power_log = millis();
    }
}
