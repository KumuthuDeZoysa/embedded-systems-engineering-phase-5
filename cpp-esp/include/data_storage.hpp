    void clearSamples();
#pragma once
#include "ticker_fallback.hpp"
#include "types.hpp"
#include <cstdint>

class SampleBuffer {
public:
    SampleBuffer(size_t max_samples = 512);
    ~SampleBuffer();
    bool append(const Sample& s);
    int queryByTime(uint32_t start_ts, uint32_t end_ts, Sample* outBuf, size_t outBufSize);
    void clear();
    size_t size() const;
    // Public accessors for buffer internals
    const Sample* getBuffer() const { return buffer_; }
    size_t getTail() const { return tail_; }
    size_t getHead() const { return head_; }
    size_t getMaxSamples() const { return max_samples_; }
private:
    Sample* buffer_;
    size_t max_samples_;
    size_t head_;
    size_t tail_;
    bool full_;
    friend class DataStorage; // Allow DataStorage to access lock/unlock
    void lock(); 
    void unlock(); 
};

class DataStorage {
public:
    DataStorage(const char* filename = "/data/samples.csv");
    ~DataStorage();

    bool appendSample(uint32_t timestamp, uint8_t reg_addr, float value);    
    int readLastSamples(int n, Sample* outBuf, size_t outBufSize);
    bool clearStorage();
    int querySamplesByTime(uint32_t start_ts, uint32_t end_ts, char* outBuf, size_t outBufSize);    
    void loop();
    size_t getBufferCapacity() const { return sample_buffer_.getMaxSamples(); }
    void clearSamples();

private:
    const char* filename;
    SampleBuffer sample_buffer_;
    Ticker flushTicker_;
    void flushTask();
    void flushBufferToFile();
    static void flushTaskWrapper();
    static DataStorage* instance_;
};
