#pragma once
#include <cstdint>
#include <string>

using RegisterAddress = uint8_t;
using RegisterValue = uint16_t;

struct Register {
    RegisterAddress addr = 0;
    RegisterValue value = 0;
    std::string name;
    std::string unit;
    float gain = 1.0f;
};

struct Sample {
    uint32_t timestamp;
    uint8_t reg_addr;
    float value;
};
