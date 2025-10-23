#pragma once

// Minimal millis()-based fallback for Ticker if sstaub/Ticker is not available.
// Provides a tiny subset: constructor, interval(), start(), stop(), update().

#ifndef HAS_TICKER_LIB
#include <Arduino.h>
#include <stdint.h>

class Ticker {
public:
    using callback_t = void(*)();
    Ticker(callback_t cb = nullptr, uint32_t interval = 0)
        : cb_(cb), interval_(interval), running_(false), last_ms_(0) {}

    void attach(callback_t cb) { cb_ = cb; }
    void interval(uint32_t ms) { interval_ = ms; }
    void start() { running_ = true; last_ms_ = millis(); }
    void stop() { running_ = false; }
    void update() { if (!running_ || !cb_) return; uint32_t now = millis(); if ((uint32_t)(now - last_ms_) >= interval_) { last_ms_ = now; cb_(); } }

private:
    callback_t cb_;
    uint32_t interval_;
    bool running_;
    uint32_t last_ms_;
};

#endif
