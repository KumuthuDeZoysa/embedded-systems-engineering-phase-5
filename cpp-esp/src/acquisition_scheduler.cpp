// Arduino helpers for millis/delay
#include <Arduino.h>
#include "ticker_fallback.hpp"
#include "../include/acquisition_scheduler.hpp"
#include "../include/protocol_adapter.hpp"
#include "../include/data_storage.hpp"
#include "../include/config_manager.hpp"
#include "../include/logger.hpp"
// --- Heap/Stack debug print helper ---
static void printMemoryStats(const char* tag) {
    // Temporarily disabled to prevent stack overflow during demo
    // Logger::info("[MEM] %s | Free heap: %u bytes | Min heap: %u bytes", tag, ESP.getFreeHeap(), ESP.getMinFreeHeap());
    // char dummy;
    // Logger::info("[MEM] %s | Stack ptr: %p", tag, &dummy);
}

AcquisitionScheduler* AcquisitionScheduler::instance_ = nullptr;

AcquisitionScheduler::AcquisitionScheduler(ProtocolAdapter* adapter, DataStorage* storage, ConfigManager* config)
    : pollTicker_(pollTaskWrapper, 1000), printTicker_(printTaskWrapper, 15000), adapter_(adapter), storage_(storage), config_(config) {
    instance_ = this;
}
AcquisitionScheduler::~AcquisitionScheduler() { end(); }

void AcquisitionScheduler::begin(uint32_t interval_ms) {
    pollInterval_ = interval_ms;
    pollTicker_.interval(pollInterval_);
    printTicker_.interval(printInterval_);
    // Ensure regList_ is initialized with all registers if empty
    if (regList_.empty()) {
        regList_ = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    }
    if (pollInterval_ > 0) {
        running_ = true;
        pollTicker_.start();
        printTicker_.start();
    } else {
        running_ = false;
    }
}

void AcquisitionScheduler::end() {
    pollTicker_.stop();
    printTicker_.stop();
    running_ = false;
}

void AcquisitionScheduler::updateConfig(const uint8_t* reg_list, size_t reg_count, uint32_t interval_ms) {
    regList_.assign(reg_list, reg_list + reg_count);
    if (pollInterval_ != interval_ms) {
        pollInterval_ = interval_ms;
        pollTicker_.interval(pollInterval_);
    }
}

void AcquisitionScheduler::loop() {
    pollTicker_.update();
    printTicker_.update();
}

void AcquisitionScheduler::pollTask() {
    if (!running_ || regList_.empty()) return;
    printMemoryStats("AcqPollTask");
    // Clear sample buffer before each acquisition cycle to avoid accumulation
    storage_->clearSamples();
    Logger::info("Acquisition loop: regList_ size=%u", (unsigned)regList_.size());
    for (uint8_t reg : regList_) {
        Logger::info("Acquisition loop: reg=%d", reg);
        uint16_t raw_value = 0;
        ModbusConfig mb = config_->getModbusConfig();
        int tries = mb.max_retries ? mb.max_retries : 3;
        int attempt = 0;
        bool ok = false;
        while (attempt < tries && !ok) {
            if (adapter_ && adapter_->readRegisters(reg, 1, &raw_value)) {
                ok = true;
                break;
            }
            attempt++;
            if (mb.retry_delay_ms) delay(mb.retry_delay_ms);
        }
        if (!ok) {
            Logger::warn("Acquisition: Failed to read register %d after %d attempts. Skipping.", reg, tries);
            continue;
        }
        uint32_t ts = millis();
        RegisterConfig reg_config = config_->getRegisterConfig(reg);
        float gain = reg_config.gain;
        if (gain <= 0.0f) {
            Logger::warn("Acquisition: register %d has invalid gain %f, using 1.0", reg, gain);
            gain = 1.0f;
        }
        float final_value = (float)raw_value / gain;
        storage_->appendSample(ts, reg, final_value);
        Logger::debug("Acquisition: Register %d value: %f", reg, final_value);
        recentSamples_.push_back({ts, reg, final_value});
        if (recentSamples_.size() > 10) {
            recentSamples_.erase(recentSamples_.begin());
        }
    }
}

void AcquisitionScheduler::printTaskWrapper() {
    if (instance_) {
        instance_->printTask();
    }
}

void AcquisitionScheduler::pollTaskWrapper() {
    if (instance_) {
        instance_->pollTask();
    }
}

void AcquisitionScheduler::printTask() {
    if (recentSamples_.empty()) return;
    Logger::info("=== Buffer Output (all samples) ===");
    for (const auto& sample : recentSamples_) {
        RegisterConfig reg_config = config_->getRegisterConfig(sample.reg_addr);
        Logger::info("Reg %d: %.2f %s", sample.reg_addr, sample.value, reg_config.unit.c_str());
    }
    
    Logger::info("=== End Buffer Output ===");
    
    // Clear recent samples after printing
    recentSamples_.clear();
}

void AcquisitionScheduler::getStatistics(char* outBuf, size_t outBufSize) const {
    snprintf(outBuf, outBufSize, "interval=%lu, regs=%u, running=%d", (unsigned long)pollInterval_, (unsigned int)regList_.size(), running_);
}
