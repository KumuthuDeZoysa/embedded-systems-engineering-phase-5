# 📋 DEMO QUICK REFERENCE CARD
## Keep this open during your live demonstration

---

## 🚀 STARTUP COMMANDS (Run in order)

### Terminal 1: Start Flask Server
```powershell
cd "e:\EMBEDDED PHASE 5\embedded-systems-engineering-phase-5\cpp-esp"
python app.py
```

### Terminal 2: Upload & Monitor ESP32
```powershell
cd "e:\EMBEDDED PHASE 5\embedded-systems-engineering-phase-5\cpp-esp"
pio run -t upload
pio device monitor --baud 115200
```

---

## 🎯 DEMO COMMANDS BY FEATURE

### ✅ Feature 3: Remote Configuration
```powershell
# Change sampling interval to 10 seconds
curl -X POST "http://localhost:8080/api/cloud/config/push" -H "Content-Type: application/json" -d "{\"device_id\": \"EcoWatt001\", \"sampling_interval_ms\": 10000}"
```

### ✅ Feature 4: Command Execution
```powershell
# Write to Export_power_percentage register
curl -X POST "http://localhost:8080/api/cloud/command/queue" -H "Content-Type: application/json" -d "{\"device_id\": \"EcoWatt001\", \"action\": \"write_register\", \"target_register\": \"Export_power_percentage\", \"value\": 80}"
```

### ✅ Feature 5: FOTA Success
```powershell
# Upload test firmware
python upload_firmware.py

# Trigger FOTA
curl -X POST "http://localhost:8080/api/cloud/fota/trigger" -H "Content-Type: application/json" -d "{\"device_id\": \"EcoWatt001\"}"
```

### ✅ Feature 6: FOTA Rollback
```powershell
# Upload bad firmware (wrong hash)
curl -X POST "http://localhost:8080/api/cloud/fota/manifest" -H "Content-Type: application/json" -d "{\"version\": \"1.2.0-bad\", \"size\": 850000, \"hash\": \"0000000000000000000000000000000000000000000000000000000000000000\", \"chunk_size\": 1024, \"total_chunks\": 831}"

# Trigger FOTA (will fail and rollback)
curl -X POST "http://localhost:8080/api/cloud/fota/trigger" -H "Content-Type: application/json" -d "{\"device_id\": \"EcoWatt001\"}"
```

### ✅ Feature 8: Network Fault
```powershell
# STOP Flask server (Ctrl+C in terminal 1)
# Wait for device to show retry logs
# RESTART Flask server:
python app.py
```

### ✅ Feature 9: Inverter SIM Fault
```powershell
# Inject timeout (device will timeout waiting for response)
curl -X POST "http://localhost:8080/api/inverter/fault/inject" -H "Content-Type: application/json" -d "{\"fault_type\": \"timeout\"}"

# Inject bad CRC (device will detect CRC mismatch)
curl -X POST "http://localhost:8080/api/inverter/fault/inject" -H "Content-Type: application/json" -d "{\"fault_type\": \"bad_crc\"}"

# Inject malformed frame (device will detect truncated response)
curl -X POST "http://localhost:8080/api/inverter/fault/inject" -H "Content-Type: application/json" -d "{\"fault_type\": \"malformed\"}"

# Check fault status
curl "http://localhost:8080/api/inverter/fault/status"

# Clear ALL faults (back to normal)
curl -X POST "http://localhost:8080/api/inverter/fault/clear"
```

---

## 🔍 WHAT TO LOOK FOR IN SERIAL MONITOR

### Data Acquisition:
```
[INFO] [Acquisition] Reading registers...
[DEBUG] [DataStorage] Buffered sample: ts=..., reg=0, val=230.0
```

### Secure Upload:
```
[INFO] [Security] Message secured: nonce=..., mac=...
[INFO] [SecureHTTP] Response: 200 OK
```

### Config Update:
```
[INFO] [ConfigMgr] Parameter accepted: sampling_interval 5000 -> 10000
```

### Command Execution:
```
[INFO] [CmdExec] Command 1 executed: status=SUCCESS
```

### FOTA:
```
[INFO] [FOTA] New firmware available: 1.1.0
[INFO] [FOTA] Hash verified ✓
```

### Faults:
```
[WARN] [FaultHandler] Network error / Inverter SIM timeout
[INFO] [EventLog] FAULT logged: ...
[INFO] [FaultHandler] Recovery successful
```

### Power:
```
[INFO] [PowerMgr] Stats: Mode=NORMAL, CPU=160MHz, WiFi_Sleep=true
```

---

## 💬 KEY PHRASES TO SAY

| Feature | What to Say |
|---------|-------------|
| Acquisition | "Polling every 5 seconds, storing in 512-sample ring buffer" |
| Security | "HMAC-SHA256 authentication, nonce anti-replay protection" |
| Config | "Runtime update without reboot, persisted to flash" |
| Command | "Queue-based flow: Cloud → Device → Inverter SIM → Result" |
| FOTA Success | "Chunked download, SHA-256 verification, dual OTA partitions" |
| FOTA Rollback | "Automatic rollback on hash mismatch or boot failure" |
| Power | "87.5% savings: CPU scaling + WiFi sleep + peripheral gating" |
| Network Fault | "Exponential backoff retry, degraded mode, data buffered" |
| SIM Fault | "CRC validation, timeout detection, graceful recovery" |

---

## ⚠️ IF SOMETHING GOES WRONG

| Problem | Quick Fix |
|---------|-----------|
| WiFi won't connect | Press EN button to reboot ESP32 |
| Flask errors | Kill python.exe in Task Manager, restart |
| Serial monitor blank | Close and reopen, check COM port |
| FOTA stuck | Reboot ESP32, it will resume or rollback |
| Commands not received | Check device_id matches "EcoWatt001" |

---

## 📊 POWER NUMBERS TO REMEMBER

```
BASELINE:  160 mA @ 528 mW (240 MHz, no sleep)
OPTIMIZED:  20 mA @  66 mW (160 MHz, WiFi sleep)
SAVINGS:   87.5% reduction
```

---

**Good luck! You've got this! 🎉**
