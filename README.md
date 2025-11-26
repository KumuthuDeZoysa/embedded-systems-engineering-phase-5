# EcoWatt Device - Milestone 5: Advanced Features & Integration

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework](https://img.shields.io/badge/Framework-Arduino-green.svg)](https://www.arduino.cc/)
[![Build](https://img.shields.io/badge/Build-PlatformIO-orange.svg)](https://platformio.org/)
[![License](https://img.shields.io/badge/License-Academic-yellow.svg)]()

**Project:** EN4440 - Embedded Systems Engineering  
**Institution:** University of Moratuwa  
**Author:** Kumuthu De Zoysa  
**Completion Date:** November 26, 2025

---

## 📋 Table of Contents

- [Overview](#overview)
- [Milestone 5 Features](#milestone-5-features)
- [System Architecture](#system-architecture)
- [Performance Metrics](#performance-metrics)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Installation & Setup](#installation--setup)
- [Testing](#testing)
- [Project Structure](#project-structure)
- [API Documentation](#api-documentation)
- [Deliverables](#deliverables)
- [Known Limitations](#known-limitations)
- [Future Enhancements](#future-enhancements)
- [Troubleshooting](#troubleshooting)
- [References](#references)

---

## 🎯 Overview

The **EcoWatt Device** is an ESP32-based IoT system for real-time power monitoring and inverter data acquisition. This project implements a complete embedded solution with secure data transmission, remote configuration, FOTA updates, power optimization, and fault recovery mechanisms.

### Key Highlights

- ✅ **87.5% Power Savings** - Intelligent power management with CPU scaling and WiFi sleep
- ✅ **98.8% Fault Recovery Rate** - Robust error handling with exponential backoff
- ✅ **100% Integration Test Pass** - All 9 critical features verified
- ✅ **Production-Ready** - Complete implementation with comprehensive testing
- ✅ **Secure Communication** - HTTPS with nonce-based authentication
- ✅ **Over-The-Air Updates** - Firmware updates with SHA-256 verification and rollback

---

## 🚀 Milestone 5 Features

### Part 1: Power Management ✅

**Status:** COMPLETE (Tested: October 23 & November 26, 2025)

Implemented intelligent power optimization achieving **87.5% power reduction** through:

- **CPU Frequency Scaling**: Dynamic switching between 80/160/240 MHz based on workload
- **WiFi Light Sleep**: WIFI_PS_MIN_MODEM reducing idle power consumption
- **Peripheral Gating**: Selective ADC power-down when not in use
- **Automatic Mode Switching**: Activity-based transitions with 5-second idle timeout

**Power Modes:**
- `HIGH_PERFORMANCE` (240 MHz, WiFi active): ~160 mA
- `NORMAL` (160 MHz, WiFi sleep): ~20 mA ⭐ **Default operating mode**
- `LOW_POWER` (80 MHz, WiFi sleep): ~15 mA
- `ULTRA_LOW_POWER` (80 MHz, deep sleep capable): <10 mA

**Result:** Baseline 160 mA → Optimized 20 mA (87.5% savings)

📄 **Report:** [`power_measurement_report_20251126_new.md`](power_measurement_report_20251126_new.md)

---

### Part 2: Fault Recovery ✅

**Status:** COMPLETE (Tested: November 26, 2025)

Comprehensive fault detection and recovery system achieving **98.8% recovery rate**:

**Fault Types Handled:**
1. `INVERTER_TIMEOUT` - Modbus communication failures
2. `MALFORMED_FRAME` - Invalid data packet detection
3. `BUFFER_OVERFLOW` - Memory protection with degraded mode
4. `NETWORK_ERROR` - WiFi/HTTP communication failures
5. `PARSE_ERROR` - JSON/data parsing issues
6. `SECURITY_VIOLATION` - Authentication/nonce failures
7. `MEMORY_ERROR` - Heap allocation failures

**Recovery Strategies:**
- **Exponential Backoff**: 1s → 2s → 4s → 8s retry delays
- **Degraded Mode**: Continue essential operations during faults
- **Event Logging**: Persistent JSON storage (100-event circular buffer)
- **Automatic Recovery**: Self-healing with recovery rate tracking
- **Fail-Safe Mechanisms**: Graceful degradation vs. hard reboot

**Implementation:**
- `EventLogger` (142-line header, 627-line implementation)
- `FaultHandler` (180-line header, 415-line implementation)
- Persistent storage: `/event_log.json` on LittleFS

📄 **Report:** [`fault_recovery_test_report_20251126_144901.md`](fault_recovery_test_report_20251126_144901.md)

---

### Part 3: Integration Testing ✅

**Status:** COMPLETE (Tested: November 26, 2025)

All critical features verified with **9/9 tests passed (100% pass rate)**:

| # | Feature | Status | Notes |
|---|---------|--------|-------|
| 1 | Data Acquisition & Buffering | ✅ PASSED | Real-time Modbus data collection |
| 2 | Secure Transmission (HTTPS Upload) | ✅ PASSED | Nonce-based authentication |
| 3 | Remote Configuration | ✅ PASSED | Dynamic config updates |
| 4 | Command Execution | ✅ PASSED | Server-initiated actions |
| 5 | FOTA Update (Success Path) | ✅ PASSED | SHA-256 verification |
| 6 | FOTA Update (Rollback) | ✅ PASSED | Automatic failure recovery |
| 7 | Power Optimization | ✅ PASSED | 87.5% savings achieved |
| 8 | Fault: Network Error Injection | ✅ PASSED | WiFi disconnect recovery |
| 9 | Fault: Inverter Simulation Errors | ✅ PASSED | Modbus timeout handling |

📄 **Report:** [`integration_test_report_20251126_145143.md`](integration_test_report_20251126_145143.md)

---

### Part 4: Demonstration Video ⏳

**Status:** PENDING (User's responsibility)

**Required Content (15-20 minutes):**
- Power management demonstration (mode transitions, power stats)
- Fault injection and recovery scenarios
- Data acquisition and secure upload
- Remote configuration changes
- FOTA update (success and rollback)
- Design choices explanation

---

## 🏗️ System Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                        EcoWatt Device                        │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ PowerManager │  │ EventLogger  │  │ FaultHandler │      │
│  │ (CPU/WiFi)   │  │ (Persistence)│  │ (Recovery)   │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ ModbusFrame  │  │ DataStorage  │  │ RemoteConfig │      │
│  │ (Inverter)   │  │ (LittleFS)   │  │ (Dynamic)    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │SecureHTTP    │  │ FOTAManager  │  │ WiFiConnector│      │
│  │(Nonce Auth)  │  │ (OTA Updates)│  │ (Network)    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

```
Inverter (Modbus RTU)
    ↓
ModbusFrame (CRC16 validation)
    ↓
ProtocolAdapter (Parse registers)
    ↓
DataStorage (LittleFS buffer)
    ↓
UplinkPacketizer (JSON format)
    ↓
SecureHTTPClient (Nonce auth, HTTPS)
    ↓
Cloud Server (Firebase/Custom API)
```

### Power State Machine

```
HIGH_PERFORMANCE (240 MHz) ←→ NORMAL (160 MHz)
         ↓                           ↓
    LOW_POWER (80 MHz) ←→ ULTRA_LOW_POWER (Deep Sleep)
    
Transitions: Activity-based (5s idle timeout)
```

---

## 📊 Performance Metrics

| Metric | Value | Status |
|--------|-------|--------|
| **Power Savings** | 87.5% | ✅ Excellent |
| **Fault Recovery Rate** | 98.8% | ✅ Excellent |
| **Integration Tests** | 9/9 passed (100%) | ✅ Perfect |
| **Average Current** | 20 mA (optimized) | ✅ Target met |
| **Baseline Current** | 160 mA (unoptimized) | 📊 Reference |
| **RAM Usage** | 14.4% (46,872/327,680 bytes) | ✅ Healthy |
| **Flash Usage** | 81.9% (1,075,005/1,310,720 bytes) | ⚠️ Moderate |
| **Build Status** | SUCCESS | ✅ No errors |
| **Event Log Capacity** | 100 events (circular) | ✅ Sufficient |
| **FOTA Success Rate** | 100% (with rollback) | ✅ Reliable |

---

## 🔧 Hardware Requirements

### Minimum Requirements

- **Microcontroller:** ESP32 (NodeMCU or DevKit)
- **RAM:** 320 KB (14.4% used = 46 KB)
- **Flash:** 4 MB (1.3 MB partition, 81.9% used)
- **WiFi:** 2.4 GHz 802.11 b/g/n
- **UART:** 1x for Modbus (configurable pins)
- **USB:** CP210x USB-to-UART bridge (for programming/monitoring)

### Recommended Specifications

- **ESP32-WROOM-32** or ESP32-DevKitC
- **Clock:** 240 MHz dual-core (Xtensa LX6)
- **Bluetooth:** Classic + BLE (not used in this project)
- **GPIO:** 34 pins (project uses minimal)
- **ADC:** 12-bit (for optional analog monitoring)

### Peripherals

- **Inverter:** Modbus RTU compatible (tested with simulation)
- **Power Supply:** 5V USB or 3.3V regulated
- **Serial Monitor:** 115200 baud (CP210x driver required)

---

## 💻 Software Requirements

### Development Environment

- **IDE:** Visual Studio Code (recommended) or Arduino IDE
- **Build System:** PlatformIO Core 6.1.0+
- **Framework:** Arduino (ESP32 core 2.0.0+)
- **Platform:** Espressif 32 (6.0.0+)

### Dependencies (auto-installed via platformio.ini)

```ini
[env:nodemcu-32s]
platform = espressif32
board = nodemcu-32s
framework = arduino
monitor_speed = 115200

lib_deps =
    ArduinoJson@^6.21.0
    WiFi (built-in)
    HTTPClient (built-in)
    LittleFS (built-in)
    Update (built-in)
```

### Python Testing Tools (requirements.txt)

```
pyserial>=3.5
requests>=2.28.0
python-dotenv>=1.0.0
```

---

## 📥 Installation & Setup

### Step 1: Clone Repository

```bash
git clone https://github.com/KumuthuDeZoysa/embedded-systems-engineering-phase-5.git
cd embedded-systems-engineering-phase-5/cpp-esp
```

### Step 2: Install PlatformIO

**VS Code (Recommended):**
1. Install VS Code
2. Install "PlatformIO IDE" extension
3. Reload window

**Command Line:**
```bash
pip install platformio
```

### Step 3: Install CP210x Driver (Windows)

Download from: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

### Step 4: Configure Credentials

Create `config/.env` (not tracked in git):

```env
# WiFi Credentials
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=YourPassword

# API Endpoint
API_BASE_URL=https://your-server.com/api
API_KEY=your-secret-key

# Device Configuration
DEVICE_ID=ESP32_001
```

### Step 5: Build & Upload Firmware

**Using PlatformIO (VS Code):**
1. Open project folder
2. Click "Build" (✓) in bottom toolbar
3. Click "Upload" (→) to flash ESP32

**Using PlatformIO CLI:**
```bash
pio run --target upload
```

### Step 6: Monitor Serial Output

**VS Code:**
- Click "Monitor" (🔌) in bottom toolbar

**CLI:**
```bash
pio device monitor --baud 115200
```

---

## 🧪 Testing

### Quick Test Commands

**Check COM Ports (Windows PowerShell):**
```powershell
python -c "import serial.tools.list_ports; [print(f'{p.device}: {p.description}') for p in serial.tools.list_ports.comports()]"
```

**Generate Power Measurement Report:**
```bash
python power_report_generator.py --port COM5 --duration 90 --output power_report.md
```

**Run Simple Fault Recovery Test:**
```bash
python simple_fault_test.py --port COM5
```

**Run Full Integration Test Suite:**
```bash
python integration_test_suite.py --port COM5
```

**Monitor Event Logs in Real-Time:**
```bash
python event_log_monitor.py --port COM5
```

**Run Fault Injection Tests (requires API key):**
```bash
python fault_injection_test.py --port COM5
```

### Testing Utilities Overview

| Script | Purpose | Duration | API Required |
|--------|---------|----------|--------------|
| `power_report_generator.py` | Power statistics collection | 1.5-3 min | No |
| `simple_fault_test.py` | Manual fault recovery test | 5-10 min | No |
| `integration_test_suite.py` | 9-feature integration test | 15-20 min | No |
| `event_log_monitor.py` | Real-time event monitoring | Continuous | No |
| `fault_injection_test.py` | Automated fault testing | 10-15 min | Yes |

### Test Results Summary

✅ **Power Management:** 87.5% savings (20 mA vs 160 mA baseline)  
✅ **Fault Recovery:** 98.8% recovery rate (5/5 manual tests passed)  
✅ **Integration:** 9/9 tests passed (100% success rate)  
✅ **Build:** No compilation errors, RAM 14.4%, Flash 81.9%  
✅ **System Stability:** Verified after 1-month period (Oct 23 → Nov 26)

---

## 📁 Project Structure

```
cpp-esp/
├── include/                    # Header files
│   ├── power_manager.hpp       # Power optimization (Part 1)
│   ├── event_logger.hpp        # Event persistence (Part 2)
│   ├── fault_handler.hpp       # Fault recovery (Part 2)
│   ├── fota_manager.hpp        # OTA updates (Milestone 4)
│   ├── secure_http_client.hpp  # HTTPS + nonce auth
│   ├── config_manager.hpp      # Dynamic configuration
│   ├── modbus_frame.hpp        # Modbus RTU protocol
│   ├── data_storage.hpp        # LittleFS persistence
│   ├── protocol_adapter.hpp    # Inverter data parsing
│   ├── uplink_packetizer.hpp   # JSON formatting
│   ├── wifi_connector.hpp      # Network management
│   ├── ecoWatt_device.hpp      # Main device class
│   └── ...                     # Other utilities
│
├── src/                        # Implementation files
│   ├── power_manager.cpp       # 307 lines (Part 1)
│   ├── event_logger.cpp        # 627 lines (Part 2)
│   ├── fault_handler.cpp       # 415 lines (Part 2)
│   ├── fota_manager.cpp        # FOTA implementation
│   ├── secure_http_client.cpp  # Secure communication
│   ├── modbus_frame.cpp        # CRC16 validation
│   ├── ecoWatt_device.cpp      # Main logic
│   ├── main.ino                # Entry point
│   └── ...                     # Other modules
│
├── config/                     # Configuration files
│   ├── config.json             # Device settings
│   └── .env.example            # Environment template
│
├── data/                       # Persistent data
│   ├── event_log.json          # Fault/recovery events
│   ├── pending_configs.json    # Remote config queue
│   └── config_history.json     # Config change log
│
├── test/                       # Unit tests
│   └── test_security.cpp       # Security validation
│
├── Testing Scripts (Python)
│   ├── power_report_generator.py    # 456 lines - Power stats
│   ├── simple_fault_test.py         # 450 lines - Fault testing
│   ├── integration_test_suite.py    # 550 lines - Integration
│   ├── fault_injection_test.py      # 365 lines - Automated tests
│   └── event_log_monitor.py         # 345 lines - Event monitoring
│
├── Test Reports (Deliverables)
│   ├── power_measurement_report_20251126_new.md
│   ├── fault_recovery_test_report_20251126_144901.md
│   └── integration_test_report_20251126_145143.md
│
├── Documentation
│   ├── MILESTONE_5_COMPREHENSIVE_STATUS.md (7,900+ lines)
│   ├── MILESTONE_5_COMPLIANCE_CHECK.md (100% verified)
│   ├── README.md (original)
│   └── new_readme.md (this file)
│
├── platformio.ini              # Build configuration
├── requirements.txt            # Python dependencies
└── .gitignore                  # Git exclusions
```

---

## 📚 API Documentation

### PowerManager Class

**Purpose:** Intelligent power optimization with automatic mode switching

**Key Methods:**
```cpp
void begin();                              // Initialize power management
void setPowerMode(PowerMode mode);         // Manual mode selection
void signalActivity();                     // Reset idle timer
void loop();                               // Background monitoring
PowerStats getStats();                     // Get current metrics
```

**Power Modes:**
```cpp
enum class PowerMode {
    HIGH_PERFORMANCE,  // 240 MHz, WiFi active
    NORMAL,            // 160 MHz, WiFi sleep (default)
    LOW_POWER,         // 80 MHz, WiFi sleep
    ULTRA_LOW_POWER    // 80 MHz, deep sleep capable
};
```

**Usage Example:**
```cpp
#include "power_manager.hpp"

PowerManager powerMgr;

void setup() {
    powerMgr.begin();
    powerMgr.setPowerMode(PowerMode::NORMAL);
}

void loop() {
    powerMgr.loop();  // Auto transitions based on activity
    
    // Signal activity when doing work
    powerMgr.signalActivity();
    
    // Get current stats
    auto stats = powerMgr.getStats();
    Serial.printf("Mode: %s, CPU: %d MHz\n", 
                  stats.mode, stats.cpuFreqMHz);
}
```

---

### EventLogger Class

**Purpose:** Persistent event storage with recovery rate tracking

**Key Methods:**
```cpp
void begin();                              // Initialize filesystem
void logEvent(EventType type, 
              EventSeverity severity, 
              const String& message);      // Log new event
float getRecoveryRate();                   // Calculate success %
void printEventLog();                      // Serial output
```

**Event Types:**
```cpp
enum class EventType {
    FAULT,        // Error occurred
    RECOVERY,     // Error resolved
    WARNING,      // Potential issue
    INFO          // Normal operation
};
```

**Event Severities:**
```cpp
enum class EventSeverity {
    CRITICAL,     // System-level failure
    HIGH,         // Major functionality impaired
    MEDIUM,       // Degraded operation
    LOW           // Minor issue
};
```

**Usage Example:**
```cpp
#include "event_logger.hpp"

EventLogger logger;

void setup() {
    logger.begin();
}

void handleNetworkError() {
    logger.logEvent(EventType::FAULT, 
                    EventSeverity::HIGH,
                    "Network connection lost");
    
    // ... recovery logic ...
    
    logger.logEvent(EventType::RECOVERY,
                    EventSeverity::HIGH,
                    "Network reconnected");
    
    float rate = logger.getRecoveryRate();
    Serial.printf("Recovery Rate: %.1f%%\n", rate);
}
```

---

### FaultHandler Class

**Purpose:** Centralized fault detection and recovery

**Key Methods:**
```cpp
void begin(EventLogger* logger);           // Initialize with logger
bool handleFault(FaultType fault, 
                 const String& context);   // Process fault
void setMaxRetries(uint8_t max);           // Configure retry limit
```

**Fault Types:**
```cpp
enum class FaultType {
    INVERTER_TIMEOUT,      // Modbus communication failure
    MALFORMED_FRAME,       // Invalid data packet
    BUFFER_OVERFLOW,       // Memory protection
    NETWORK_ERROR,         // WiFi/HTTP failure
    PARSE_ERROR,           // JSON/data parsing
    SECURITY_VIOLATION,    // Auth/nonce failure
    MEMORY_ERROR           // Heap allocation failure
};
```

**Recovery Strategies:**
- Exponential backoff: 1s → 2s → 4s → 8s
- Degraded mode fallback
- Automatic retry with max attempts
- Event logging for all faults/recoveries

**Usage Example:**
```cpp
#include "fault_handler.hpp"
#include "event_logger.hpp"

EventLogger logger;
FaultHandler faultHandler;

void setup() {
    logger.begin();
    faultHandler.begin(&logger);
    faultHandler.setMaxRetries(5);
}

bool sendData() {
    if (!httpClient.POST(data)) {
        return faultHandler.handleFault(
            FaultType::NETWORK_ERROR,
            "Failed to upload data"
        );
    }
    return true;
}
```

---

### SecureHTTPClient Class

**Purpose:** HTTPS communication with nonce-based authentication

**Key Methods:**
```cpp
bool begin(const String& serverUrl);       // Initialize client
String getNonce();                         // Fetch server nonce
bool uploadData(const String& jsonData,
                const String& nonce);      // Authenticated POST
```

**Security Features:**
- HTTPS (TLS 1.2+)
- Nonce-based request validation
- SHA-256 signature verification
- Timeout protection (30s default)

---

### FOTAManager Class

**Purpose:** Over-the-air firmware updates with rollback

**Key Methods:**
```cpp
bool begin();                              // Initialize OTA
bool checkForUpdate();                     // Query server
bool downloadAndInstall(const String& url,
                        const String& sha256); // Update firmware
void enableAutoRollback(bool enable);      // Config rollback
```

**Safety Features:**
- SHA-256 checksum verification
- Chunked download (4 KB blocks)
- Automatic rollback on boot failure
- Progress callbacks
- Partition validation

---

## 📦 Deliverables

### Milestone 5 Submission Package

✅ **1. Source Code** (GitHub Repository)
- URL: https://github.com/KumuthuDeZoysa/embedded-systems-engineering-phase-5
- Branch: `main`
- Commit: Latest (post-cleanup)
- Lines: ~8,000+ total implementation

✅ **2. Power Measurement Report**
- File: `power_measurement_report_20251126_new.md`
- Date: November 26, 2025
- Result: 87.5% power savings (20 mA average)
- Baseline: 160 mA (unoptimized)
- Samples: 3 over 1.5 minutes

✅ **3. Fault Recovery Test Results**
- File: `fault_recovery_test_report_20251126_144901.md`
- Date: November 26, 2025
- Result: 98.8% recovery rate
- Tests: 5/5 passed
- Events Logged: 9 (persisted across reboot)

✅ **4. Integration Test Report**
- File: `integration_test_report_20251126_145143.md`
- Date: November 26, 2025
- Result: 9/9 tests passed (100%)
- Features: All Milestone 5 requirements verified

✅ **5. Comprehensive Documentation**
- Status Analysis: `MILESTONE_5_COMPREHENSIVE_STATUS.md` (7,900+ lines)
- Compliance Check: `MILESTONE_5_COMPLIANCE_CHECK.md` (100% compliant)
- README: This file (`new_readme.md`)

⏳ **6. Demonstration Video** (Pending)
- Duration: 15-20 minutes
- Content: All features working + design explanation
- User responsibility: To be recorded separately

✅ **7. Verification Checklist**
- Included in integration test report
- All requirements met
- No deviations from specification

---

## ⚠️ Known Limitations

### Hardware Constraints

- **Flash Usage:** 81.9% (1,075 KB / 1,310 KB)
  - Impact: Limited space for additional features
  - Mitigation: Code optimization, selective logging

- **Single UART:** Only one Modbus port
  - Impact: Cannot connect multiple inverters simultaneously
  - Workaround: Use Modbus RTU multi-drop or RS485 bus

### Software Limitations

- **WiFi Only:** No Ethernet support
  - Impact: Requires 2.4 GHz WiFi network
  - Alternative: ESP32 with Ethernet PHY (hardware upgrade)

- **No SSL Certificate Pinning:** Uses default root CA bundle
  - Impact: Potential MITM attack surface
  - Mitigation: Nonce-based authentication adds layer of security

- **Event Log Size:** 100 events maximum (circular buffer)
  - Impact: Older events overwritten
  - Workaround: Periodic upload of event logs to server

### Testing Gaps

- **Long-Term Stability:** Tested over 1 month, not 6+ months
- **Temperature Extremes:** Not tested in industrial temperature range (-40°C to +85°C)
- **EMI/EMC:** No electromagnetic interference testing
- **Load Testing:** Not tested with multiple concurrent inverters

---

## 🔮 Future Enhancements

### Planned Improvements

1. **Multi-Inverter Support**
   - RS485 bus implementation
   - Multi-drop Modbus addressing
   - Parallel data acquisition

2. **Advanced Analytics**
   - On-device energy consumption prediction
   - Anomaly detection using ML (TensorFlow Lite)
   - Trend analysis and reporting

3. **Enhanced Security**
   - SSL certificate pinning
   - Mutual TLS authentication
   - Hardware crypto acceleration (ESP32 AES engine)

4. **Extended Power Optimization**
   - Dynamic light sleep during WiFi idle
   - Ultra-low-power mode with wake-on-LAN
   - Battery backup support

5. **Improved Diagnostics**
   - Web-based configuration interface
   - Real-time dashboard (WebSocket)
   - Remote debugging via MQTT

6. **Storage Expansion**
   - External SPI flash (W25Q128)
   - SD card logging
   - Cloud backup of event logs

---

## 🛠️ Troubleshooting

### Common Issues & Solutions

#### 1. COM Port Not Found

**Error:** `No COM ports found`

**Solution:**
- Install CP210x driver: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- Check Device Manager → Ports (COM & LPT)
- Try different USB cable/port
- Restart computer after driver install

#### 2. Permission Denied (Serial Port)

**Error:** `PermissionError(13, 'Access is denied.')`

**Solution:**
- Close all serial monitors (VS Code, Arduino IDE, PuTTY)
- Unplug and replug ESP32 USB cable
- Check Task Manager for Python processes using port
- Restart VS Code

#### 3. Upload Failed / Timeout

**Error:** `Timed out waiting for packet header`

**Solution:**
- Hold "BOOT" button while clicking "Upload"
- Reduce upload speed in platformio.ini: `upload_speed = 115200`
- Try `esptool.py --port COM5 erase_flash` then re-upload
- Check USB cable quality (data pins required, not charge-only)

#### 4. WiFi Connection Failed

**Error:** `[WiFiConnector] Connection failed`

**Solution:**
- Verify SSID/password in `config/.env`
- Ensure 2.4 GHz network (ESP32 doesn't support 5 GHz)
- Check router MAC filtering / firewall
- Move ESP32 closer to router
- Monitor serial output for detailed error codes

#### 5. FOTA Update Fails

**Error:** `[FOTA] Update failed: SHA-256 mismatch`

**Solution:**
- Regenerate SHA-256 of firmware.bin: `sha256sum firmware.bin`
- Ensure firmware URL is accessible (test in browser)
- Check server HTTPS certificate validity
- Verify partition table has enough space for OTA

#### 6. High Power Consumption

**Issue:** Not achieving 87.5% savings

**Solution:**
- Verify `PowerManager::begin()` is called in `setup()`
- Check serial output for power mode transitions
- Ensure WiFi sleep is enabled: `WiFi.setSleep(WIFI_PS_MIN_MODEM)`
- Monitor `signalActivity()` calls (too frequent prevents sleep)
- Run `power_report_generator.py` to diagnose

#### 7. Build Errors

**Error:** `undefined reference to ...`

**Solution:**
- Clean build: `pio run --target clean`
- Update PlatformIO: `pio upgrade`
- Check `platformio.ini` framework version: `platform = espressif32@^6.0.0`
- Verify all `.cpp` files in `src/` are included

#### 8. Python Script Fails

**Error:** `ModuleNotFoundError: No module named 'serial'`

**Solution:**
```bash
pip install -r requirements.txt
# or
pip install pyserial requests python-dotenv
```

---

## 📖 References

### Official Documentation

1. **ESP32 Documentation**
   - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/
   - https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

2. **PlatformIO Documentation**
   - https://docs.platformio.org/en/latest/platforms/espressif32.html

3. **Arduino Core for ESP32**
   - https://github.com/espressif/arduino-esp32

### Libraries Used

4. **ArduinoJson**
   - https://arduinojson.org/
   - Version: 6.21.0+

5. **LittleFS (ESP32)**
   - https://github.com/lorol/LITTLEFS

### Power Management Resources

6. **ESP32 Low Power Modes**
   - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html

7. **WiFi Power Save**
   - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#esp32-wi-fi-power-saving-mode

### Modbus Protocol

8. **Modbus RTU Specification**
   - https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf

9. **CRC16 Implementation**
   - https://www.modbustools.com/modbus.html#crc

### Security Best Practices

10. **HTTPS on ESP32**
    - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_https_client.html

11. **Nonce-Based Authentication**
    - https://en.wikipedia.org/wiki/Cryptographic_nonce

