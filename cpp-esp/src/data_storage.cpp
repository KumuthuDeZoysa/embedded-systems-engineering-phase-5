
#include "ticker_fallback.hpp"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include "../include/data_storage.hpp"
#include <string.h>
#include <cstdio>

// Public method to clear sample buffer
void DataStorage::clearSamples() {
    sample_buffer_.clear();
}

SampleBuffer::SampleBuffer(size_t max_samples) : max_samples_(max_samples), head_(0), tail_(0), full_(false) {
    buffer_ = new Sample[max_samples_];
}

SampleBuffer::~SampleBuffer() {
    delete[] buffer_;
}

bool SampleBuffer::append(const Sample& s) {
    lock();
    buffer_[head_] = s;
    if (full_) tail_ = (tail_ + 1) % max_samples_;
    head_ = (head_ + 1) % max_samples_;
    full_ = head_ == tail_;
    unlock();
    return true;
}

int SampleBuffer::queryByTime(uint32_t start_ts, uint32_t end_ts, Sample* outBuf, size_t outBufSize) {
    lock();
    size_t entries = size();
    size_t count = 0;
    for (size_t i = 0; i < entries; ++i) {
        const Sample& s = buffer_[(tail_ + i) % max_samples_];
        if (s.timestamp >= start_ts && s.timestamp <= end_ts) {
            if (count < outBufSize) outBuf[count++] = s;
        }
    }
    unlock();
    return (int)count;
}

void SampleBuffer::clear() {
    lock(); head_ = tail_ = 0; full_ = false; unlock();
}

size_t SampleBuffer::size() const {
    if (full_) return max_samples_;
    if (head_ >= tail_) return head_ - tail_;
    return max_samples_ - tail_ + head_;
}

void SampleBuffer::lock() { noInterrupts(); }
void SampleBuffer::unlock() { interrupts(); }

DataStorage* DataStorage::instance_ = nullptr;

DataStorage::DataStorage(const char* fname) : filename(fname), sample_buffer_(512), flushTicker_(flushTaskWrapper, 60000) {
    // Ensure instance is set before any callbacks
    instance_ = this;
    // LittleFS should already be mounted in setup(); attempt mount but allow formatting
    LittleFS.begin(true);
    // Start periodic flushes
    flushTicker_.start();
    // Restore buffer from file
    File file = LittleFS.open(filename, "r");
    if (file) {
        while (file.available()) {
            String line = file.readStringUntil('\n');
            int idx1 = line.indexOf(',');
            if (idx1 == -1) continue; // Malformed line, skip
            int idx2 = line.indexOf(',', idx1+1);
            if (idx2 == -1) continue; // Malformed line, skip

            uint32_t ts = line.substring(0, idx1).toInt();
            uint8_t reg_addr = line.substring(idx1+1, idx2).toInt();
            float value = line.substring(idx2+1).toFloat();
            Sample s = {ts, reg_addr, value};
            sample_buffer_.append(s);
        }
        file.close();
    }
}

// DataStorage destructor is implemented at end of file

bool DataStorage::clearStorage() {
    return LittleFS.remove(filename);
}

int DataStorage::querySamplesByTime(uint32_t start_ts, uint32_t end_ts, char* outBuf, size_t outBufSize) {
    File file = LittleFS.open(filename, "r");
    if (!file) return 0;
    int count = 0;
    size_t outLen = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        uint32_t ts = line.substring(0, line.indexOf(',')).toInt();
        if (ts >= start_ts && ts <= end_ts) {
            size_t lineLen = line.length();
            if (outLen + lineLen + 1 < outBufSize) {
                memcpy(outBuf + outLen, line.c_str(), lineLen);
                outBuf[outLen + lineLen] = '\n';
                outLen += lineLen + 1;
                count++;
            }
        }
    }
    if (outLen > 0) outBuf[outLen] = '\0';
    file.close();
    return count;
}

void DataStorage::flushTask() {
    flushBufferToFile();
}

void DataStorage::loop() {
    flushTicker_.update();
}

bool DataStorage::appendSample(uint32_t timestamp, uint8_t reg_addr, float value) {
    Sample s = {timestamp, reg_addr, value};
    sample_buffer_.append(s);
    return true;
}

int DataStorage::readLastSamples(int n, Sample* outBuf, size_t outBufSize) {
    sample_buffer_.lock();
    int total = (int)sample_buffer_.size();
    int count = (total < n) ? total : n;
    if ((size_t)count > outBufSize) count = (int)outBufSize;

    const Sample* buf = sample_buffer_.getBuffer();
    size_t head = sample_buffer_.getHead();
    size_t max_samples = sample_buffer_.getMaxSamples();

    // Read 'count' most recent entries in chronological order
    for (int i = 0; i < count; ++i) {
        // index of the (count - i)-th most recent element
        size_t idx = (head + max_samples - (size_t)(count - i)) % max_samples;
        outBuf[i] = buf[idx];
    }
    sample_buffer_.unlock();
    return count;
}

// Periodic flush to SPIFFS
void DataStorage::flushBufferToFile() {
    File file = LittleFS.open(filename, "w");
    if (!file) return;
    const Sample* buf = sample_buffer_.getBuffer();
    size_t tail = sample_buffer_.getTail();
    size_t max_samples = sample_buffer_.getMaxSamples();
    size_t entries = sample_buffer_.size();
    for (size_t i = 0; i < entries; ++i) {
        const Sample& s = buf[(tail + i) % max_samples];
        char linebuf[64];
        snprintf(linebuf, sizeof(linebuf), "%lu,%u,%.3f\n", (unsigned long)s.timestamp, s.reg_addr, s.value);
        file.print(linebuf);
    }
    file.close();
}

void DataStorage::flushTaskWrapper() {
    if (instance_) {
        instance_->flushTask();
    }
}

DataStorage::~DataStorage() {
    // Stop periodic flushing and attempt a final flush
    flushTicker_.stop();
    flushBufferToFile();
    instance_ = nullptr;
}
