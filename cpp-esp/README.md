# EcoWatt Device ESP32/NodeMCU Port

This project is a modular port of the original EcoWatt Device (C++17, desktop) to ESP32/NodeMCU (Arduino C++). All functionalities are preserved, but adapted for microcontroller constraints and libraries.

## Structure
- `include/` — Header files for each module
- `src/` — Implementation files
- `config/` — Configuration files (JSON, .env, etc.)
- `tests/` — Unit and integration tests

## Build System
Use PlatformIO or Arduino IDE for building and flashing. CMake is not supported on ESP32/NodeMCU.

## Modules
- Modbus frame & CRC
- HTTP client (WiFiClient, HTTPClient)
- Protocol adapter
- Config manager
- Logger
- Data storage (SPIFFS/LittleFS)
- Device abstraction
- Error handling

## How to proceed
Each module will be ported step by step, using ESP32/NodeMCU-compatible libraries and APIs. All original logic and register handling will be preserved.
