
#pragma once
#include "acquisition_scheduler.hpp"
#include "protocol_adapter.hpp"
#include "config_manager.hpp"
#include "data_storage.hpp"
#include "uplink_packetizer.hpp"
#include "remote_config_handler.hpp"
#include "command_executor.hpp"
#include "http_client.hpp"
#include "wifi_connector.hpp"
#include "security_layer.hpp"
#include "secure_http_client.hpp"
#include "fota_manager.hpp"
#include "power_manager.hpp"
#include "event_logger.hpp"
#include "fault_handler.hpp"
#include <LittleFS.h>
#include <stdint.h>

class EcoWattDevice {
public:
    EcoWattDevice();
    ~EcoWattDevice();

    void setup();
    void loop();

    // Device abstraction methods
    bool isOnline() const;
    float getReading(uint8_t reg_addr) const;
    bool setControl(uint8_t reg_addr, float value);
    void getStatistics(char* outBuf, size_t outBufSize) const;

    // Callback for remote config updates
    void onConfigUpdated();
    
    // Callback for command received
    void onCommandReceived(const CommandRequest& command);

private:
    AcquisitionScheduler* scheduler_ = nullptr;
    ProtocolAdapter* adapter_ = nullptr;
    DataStorage* storage_ = nullptr;
    UplinkPacketizer* uplink_packetizer_ = nullptr;
    ConfigManager* config_ = nullptr;
    RemoteConfigHandler* remote_config_handler_ = nullptr;
    CommandExecutor* command_executor_ = nullptr;
    EcoHttpClient* http_client_ = nullptr;
    WiFiConnector* wifi_ = nullptr;
    SecurityLayer* security_ = nullptr;
    SecureHttpClient* secure_http_ = nullptr;
    FOTAManager* fota_ = nullptr;
    PowerManager* power_mgr_ = nullptr;
    EventLogger* event_logger_ = nullptr;
    FaultHandler* fault_handler_ = nullptr;
};
