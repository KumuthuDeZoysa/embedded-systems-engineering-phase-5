#pragma once
#include <vector>

#include "ticker_fallback.hpp"
#include "types.hpp"
#include <Arduino.h>
class ProtocolAdapter;
class DataStorage;
class ConfigManager;

class AcquisitionScheduler {
public:
    AcquisitionScheduler(ProtocolAdapter* adapter, DataStorage* storage, ConfigManager* config);
    ~AcquisitionScheduler();

    void begin(uint32_t interval_ms = 1000);
    void end();
    void updateConfig(const uint8_t* reg_list, size_t reg_count, uint32_t interval_ms);
    void loop();
    void getStatistics(char* outBuf, size_t outBufSize) const;    

private:
    Ticker pollTicker_;
    Ticker printTicker_;
    uint32_t pollInterval_ = 1000;
    uint32_t printInterval_ = 15000; // 15 seconds like cpp
    std::vector<uint8_t> regList_;
    std::vector<Sample> recentSamples_;
    bool running_ = false;
    ProtocolAdapter* adapter_ = nullptr;
    DataStorage* storage_ = nullptr;
    ConfigManager* config_ = nullptr;
    void pollTask();
    void printTask();
    static void pollTaskWrapper();
    static void printTaskWrapper();
    static AcquisitionScheduler* instance_;
    // Error recovery, statistics, etc.
};
