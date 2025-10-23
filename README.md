# EcoWatt – Milestone 4: Remote Configuration, Command Execution, Security Layer & FOTA

This repository contains the Milestone 4 implementation for the EcoWatt embedded system. It delivers runtime Remote Configuration, queued Command Execution, a lightweight Security Layer (PSK + HMAC + anti‑replay), and a secure Firmware‑Over‑The‑Air (FOTA) update mechanism with resume, verification, and rollback.

- Target device: ESP32 (Arduino framework)
- Cloud: Flask API (Python)
- Demo tooling: PowerShell/Python helpers, optional Node‑RED dashboard

Quick links:
- Technical doc: `MILESTONE4_TECHNICAL_DOCUMENTATION.md`
- FOTA guide: `FOTA_IMPLEMENTATION.md`
- Update manager usage: `FOTA_UPDATE_MANAGER_GUIDE.md`
- Quick start testing: `FOTA_QUICK_START.md`
- Full testing guide: `FOTA_TESTING_COMPLETE_GUIDE.md`
- Security testing: `SECURITY_TESTING_GUIDE.md`
- Command execution doc: `MILESTONE4_PART2_DOCUMENTATION.md`
- Remote configuration doc: `MILESTONE4_PART1_DOCUMENTATION.md`
- Complete demo script: `MILESTONE_4_COMPLETE_DEMO_SCRIPT.md`

## ✅ What’s implemented

- Remote Configuration
  - Runtime config updates applied without reboot
  - Validation, idempotency, persistence (LittleFS)
  - Thread‑safe, non‑blocking application
  - Cloud acks and history

- Command Execution
  - Cloud queues commands → device executes on the Inverter SIM
  - Result reported back and logged
  - Full history and auditability

- Security Layer
  - PSK-based authentication with HMAC‑SHA256
  - Anti‑replay using monotonic nonces and acceptance window
  - Optional payload confidentiality
  - Secure client wrapper auto‑signs requests and verifies responses

- FOTA
  - Chunked download with resume across communication intervals
  - Per‑chunk HMAC + final SHA‑256 + ESP32 image validation
  - Controlled reboot only after successful verification
  - Automatic (boot fail) and manual (cloud flag) rollback
  - Progress, verification, and boot status reported to cloud

## 🧭 Architecture (high level)

```
Dashboard/Node‑RED → Flask API (app.py) → { Config, Commands, FOTA }
                                   ↑
                                   |
                             Secure HTTP (PSK + HMAC + Nonce)
                                   |
                           EcoWatt Device (ESP32)
  - RemoteConfigHandler → ConfigManager → AcquisitionScheduler
  - CommandExecutor → ProtocolAdapter (Inverter SIM)
  - SecurityLayer + SecureHttpClient
  - FOTAManager → OTA partition (Update.write)
```

## 📁 Key files and modules

Device (ESP32):
- Remote configuration: `src/remote_config_handler.cpp`, `src/config_manager.cpp`
- Command execution: `src/command_executor.cpp`, `include/command_execution.hpp`
- Security: `src/security_layer.cpp`, `src/secure_http_client.cpp`
- FOTA: `src/fota_manager.cpp` (direct OTA writes via `Update.write()`)

Cloud (Flask):
- API server: `app.py` (config queue/history, command queue/history, FOTA upload/manifest/chunks/status/rollback)
- Docs & scripts in repo root: see Quick Links above

## 🔌 API endpoints (representative)

Remote Configuration
- GET `/api/inverter/config` → device polls pending updates
- POST `/api/inverter/config/ack` → per‑field apply results, with reasons
- History persisted under `data/`

Command Execution
- POST `/api/cloud/command/send` → queue a command
- GET `/api/inverter/command/pending` → device fetches commands
- POST `/api/inverter/command/result` → device reports result
- GET `/api/cloud/command/history?device_id=EcoWatt001` → audit trail

FOTA
- POST `/api/cloud/fota/upload` → upload firmware; server splits/signs chunks
- GET `/api/inverter/fota/manifest` → version/size/hash/chunk info
- GET `/api/inverter/fota/chunk?chunk_number=N` → base64 data + HMAC
- POST `/api/inverter/fota/status` → progress/verify/reboot/boot_ok
- POST `/api/cloud/fota/rollback` → manual rollback request
- GET `/api/inverter/fota/rollback-status` → device checks rollback flag

Security (applies to all secured routes)
- Headers include: `Device-ID`, `X-Nonce`, `X-MAC` (HMAC over method+path+nonce+body)

## 🚀 Quick start (Windows PowerShell)

1) Start the Flask server
```powershell
python app.py
```

2) Build firmware (ESP32)
```powershell
pio run
```

3) Upload firmware to cloud (16KB chunks by default)
```powershell
python .\quick_upload.py
# or use the interactive manager:
python .\fota_update_manager.py
```

4) Trigger/observe update on device
- Press EN/reset on the ESP32 to initiate polling
- Monitor serial at 115200 to see chunked download, verify, reboot

5) Try a remote configuration change
- Use `config_dashboard.html` or send a config update via the Flask endpoint
- Device applies at runtime (next safe boundary) and persists to LittleFS
- An acknowledgment with per‑field status is logged by the server

6) Queue and observe a command execution
```powershell
# Example via PowerShell Invoke-RestMethod (or use your dashboard)
# POST /api/cloud/command/send with body:
# {"device_id":"EcoWatt001","action":"write","target_register":"Export_power_percentage","value":50}
```
- Device executes on the Inverter SIM and posts results back
- Check `/api/cloud/command/history?device_id=EcoWatt001`

## 🔐 Security guarantees

- Authentication & Integrity: PSK + HMAC‑SHA256 over canonical request
- Anti‑replay: Monotonic `X-Nonce` with server acceptance window
- Confidentiality: Optional payload encryption mode (off by default; HMAC remains mandatory)
- Key storage: PSK embedded in firmware and never logged

See: `SECURITY_TESTING_GUIDE.md`, `security_demo.py`, `security_monitor.py`

## 🔄 FOTA details

- Chunked download: 1–32KB per chunk (default 16KB)
- Resume: Bitmap/state tracks completed chunks; resumes after resets
- Verification layer:
  - Per‑chunk HMAC (authenticity)
  - Final firmware SHA‑256 (integrity)
  - `Update.end(true)` (ESP32 image validation)
- Rollback:
  - Automatic: Bootloader reverts to previous OTA on repeated boot failure
  - Manual: Cloud sets rollback flag → device switches partition on next poll
- Cloud logs: Upload → manifest served → chunk deliveries → verification → reboot → boot confirmation

See: `FOTA_IMPLEMENTATION.md`, `FOTA_QUICK_START.md`, `FOTA_TESTING_COMPLETE_GUIDE.md`

## 🧪 Minimal test flow

- Remote config: change sampling interval and register list, confirm runtime apply without reboot and persistence across power cycles
- Command execution: queue one write command, confirm device result and history entry
- Security: send a valid secured request (succeeds), then test replay or wrong MAC (rejected)
- FOTA: upload firmware, observe chunked download and verification, confirm reboot and boot_ok; then test manual rollback

## 📹 Demo script and video

- Use `MILESTONE_4_COMPLETE_DEMO_SCRIPT.md` as the presenter script
- Include the video link here: [Add your public video link]

## 🔎 Troubleshooting

- Server connectivity: ensure Flask is running and reachable from device
- HMAC failures: verify PSK matches on both device and server
- FOTA verification fails: ensure you uploaded a real ESP32 `.bin` and chunk size matches server manifest
- Device ID: confirm `Device-ID` in headers is correct (e.g., `EcoWatt001`)

## 📚 References

- Technical write‑up: `MILESTONE4_TECHNICAL_DOCUMENTATION.md`
- FOTA design & diagrams: `FOTA_IMPLEMENTATION.md`
- Update manager: `FOTA_UPDATE_MANAGER_GUIDE.md`
- Testing: `FOTA_QUICK_START.md`, `FOTA_TESTING_COMPLETE_GUIDE.md`
- Security: `SECURITY_TESTING_GUIDE.md`

---

Maintainers: Team Pebble
Date: October 2025
