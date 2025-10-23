# FOTA Update Manager - Usage Guide

## Overview

The `fota_update_manager.py` script provides a comprehensive FOTA (Firmware Over-The-Air) update system with:

- ✅ Firmware upload with chunking (16KB chunks)
- ✅ Update monitoring and progress tracking
- ✅ Automatic update verification
- ✅ Rollback functionality
- ✅ Interactive menu interface
- ✅ Complete update workflow automation

---

## Quick Start

### 1. Run the Interactive Menu

```bash
python fota_update_manager.py
```

You'll see:
```
============================================================
FOTA UPDATE MANAGER
============================================================
1. Upload firmware (quick)
2. Perform complete update with verification
3. Trigger update (manual)
4. Monitor update status
5. Rollback firmware
6. Verify current firmware version
0. Exit
============================================================
```

---

## Features

### 1. Quick Upload (Option 1)

**Purpose**: Upload firmware to server without triggering update

**Steps**:
1. Select option `1`
2. Enter version (e.g., `1.0.6`)
3. Enter description (optional)
4. Firmware will be uploaded to server

**Output**:
```
============================================================
📦 UPLOADING FIRMWARE
============================================================
Version:     1.0.6
Size:        1065968 bytes (1040KB)
Hash:        8daac459055b05e7...
Chunk size:  16384 bytes (16KB)
Total chunks: 66
============================================================

✅ Upload successful!
```

---

### 2. Complete Update with Verification (Option 2) ⭐ RECOMMENDED

**Purpose**: Full automated update process

**Features**:
- Uploads firmware
- Triggers update on device
- Waits for download completion
- Verifies new firmware version
- Auto-rollback on failure

**Steps**:
1. Select option `2`
2. Enter version (e.g., `1.0.6`)
3. Enter description
4. Confirm: `yes`
5. **Press EN button on ESP32** when prompted
6. Wait for completion

**Process Flow**:
```
STEP 1: Reading firmware...
✅ Firmware loaded: 1065968 bytes (1040KB)

STEP 2: Uploading firmware...
✅ Upload successful!

STEP 3: Triggering update...
💡 Please press the EN button on your ESP32

STEP 4: Waiting for download...
⏳ Estimated time: ~132 seconds (2 min 12 sec)

STEP 5: Verifying update...
📡 Device responded!
   Current version: 1.0.6
✅ Update verified successfully!

############################################################
# ✅ UPDATE SUCCESSFUL!
############################################################
```

---

### 3. Manual Update Trigger (Option 3)

**Purpose**: Trigger update after manual firmware upload

**When to use**:
- Already uploaded firmware separately
- Want to control update timing

**Steps**:
1. Select option `3`
2. Enter device ID (or press Enter for all devices)
3. Press EN button on ESP32

---

### 4. Monitor Update Status (Option 4)

**Purpose**: Monitor ongoing FOTA update

**Steps**:
1. Select option `4`
2. Enter timeout in seconds (default: 300)
3. Monitor progress

**Output**:
```
============================================================
📊 MONITORING UPDATE STATUS
============================================================
Timeout: 300 seconds (5 minutes)
============================================================

⏱️  Elapsed: 00:10 - Monitoring...
⏱️  Elapsed: 00:20 - Monitoring...
```

---

### 5. Rollback Firmware (Option 5) 🔄

**Purpose**: Revert to previous firmware version

**When to use**:
- New firmware has bugs
- Update verification failed
- Device behaving incorrectly

**Steps**:
1. Select option `5`
2. Enter rollback reason
3. Rollback will be triggered

**Output**:
```
============================================================
🔄 INITIATING FIRMWARE ROLLBACK
============================================================
Reason: Update verification failed
Time:   2025-10-19 12:34:56
============================================================

✅ Rollback command sent to server

📝 ROLLBACK PROCEDURE:
   1. Device will reboot
   2. Bootloader will detect rollback flag
   3. Previous firmware partition will be activated
   4. Device boots from previous working firmware
```

**How Rollback Works**:

1. **Server Side**:
   - Sets `rollback_requested` flag for device
   - Stores rollback reason

2. **Device Side**:
   - ESP32 checks rollback status periodically
   - If flag set, triggers rollback
   - Uses ESP32 OTA partition switching
   - Reboots to previous partition

3. **Automatic Recovery**:
   - If device fails to boot after update
   - ESP32 bootloader automatically rolls back
   - No manual intervention needed

---

### 6. Verify Firmware Version (Option 6)

**Purpose**: Check current firmware version on device

**Steps**:
1. Select option `6`
2. Enter expected version
3. Script checks device status

**Output**:
```
============================================================
✔️  VERIFYING UPDATE
============================================================
Expected version: 1.0.6
============================================================

📡 Device responded!
   Current version: 1.0.6
✅ Update verified successfully!
```

---

## Advanced Usage

### Programmatic Usage

You can also use the manager in your own Python scripts:

```python
from fota_update_manager import FOTAUpdateManager

# Create manager instance
manager = FOTAUpdateManager(server_url="http://localhost:8080")

# Perform complete update
manager.perform_complete_update(
    version="1.0.6",
    description="Security fixes and performance improvements",
    verify=True,
    auto_rollback=True
)
```

### Custom Firmware Path

```python
manager.perform_complete_update(
    version="1.0.7",
    firmware_path="./custom_firmware.bin",
    description="Custom build"
)
```

### Manual Steps

```python
# 1. Read firmware
firmware_data = manager.read_firmware()

# 2. Upload
manager.upload_firmware("1.0.6", firmware_data, chunk_size=16384)

# 3. Trigger update
manager.trigger_update()

# 4. Verify
if not manager.verify_update("1.0.6"):
    # 5. Rollback on failure
    manager.rollback_firmware("Verification failed")
```

---

## Server-Side Rollback Endpoints

### Trigger Rollback

**Endpoint**: `POST /api/cloud/fota/rollback`

**Request**:
```json
{
    "device_id": "optional_device_id",
    "reason": "Reason for rollback"
}
```

**Response**:
```json
{
    "success": true,
    "message": "Rollback requested for device: ESP32-001",
    "device_id": "ESP32-001",
    "reason": "Update verification failed"
}
```

### Check Rollback Status (Device)

**Endpoint**: `GET /api/inverter/fota/rollback-status`

**Headers**:
```
Device-ID: ESP32-001
```

**Response**:
```json
{
    "rollback_required": true,
    "reason": "Update verification failed"
}
```

---

## Troubleshooting

### Upload Fails

**Problem**: `❌ Upload failed: HTTP 500`

**Solution**:
1. Check Flask server is running: `python app.py`
2. Verify server URL: `http://localhost:8080`
3. Check firmware file exists: `.pio/build/esp32dev/firmware.bin`

### Verification Timeout

**Problem**: Device doesn't respond after update

**Solutions**:
1. Check serial monitor for errors
2. Manually verify device is connected to WiFi
3. Check if device rebooted properly
4. Try manual rollback if needed

### Rollback Not Working

**Problem**: Device doesn't rollback

**Manual Rollback**:
1. Power cycle ESP32
2. ESP32 bootloader detects boot failure
3. Automatic rollback to previous partition

---

## Configuration

### Change Server URL

Edit in script or pass to constructor:
```python
manager = FOTAUpdateManager(server_url="http://192.168.1.100:8080")
```

### Change Chunk Size

In `upload_firmware()` call:
```python
manager.upload_firmware(version, firmware_data, chunk_size=32768)  # 32KB chunks
```

### Adjust Timeouts

```python
# Verification timeout
manager.verify_update("1.0.6", timeout=120)  # 2 minutes

# Monitoring timeout
manager.check_update_status(timeout=600)  # 10 minutes
```

---

## Best Practices

1. **Always use complete update** (Option 2) for production
2. **Test firmware locally** before uploading
3. **Use descriptive version numbers**: `1.0.6`, `2.1.0-beta`, etc.
4. **Add meaningful descriptions**: "Security patch for CVE-2024-xxx"
5. **Monitor serial output** during update
6. **Keep rollback capability** enabled
7. **Verify updates** in production environment first

---

## Update Workflow Example

### Typical Update Session

```
1. Build new firmware
   $ pio run

2. Start Flask server (if not running)
   $ python app.py

3. Run update manager
   $ python fota_update_manager.py

4. Select option 2 (Complete update)

5. Enter details:
   Version: 1.0.6
   Description: Bug fixes and security patches

6. Confirm: yes

7. Press EN button on ESP32 when prompted

8. Watch progress in serial monitor:
   - Manifest downloaded
   - Chunks downloading (1/66, 2/66, ...)
   - Verification
   - Reboot

9. Automatic verification:
   - Script checks device version
   - Confirms update successful

10. Done! ✅
```

---

## Safety Features

1. **SHA256 Hash Verification**: Every chunk and complete firmware verified
2. **HMAC Authentication**: Prevents tampering
3. **Automatic Rollback**: On boot failure or verification failure
4. **Progress Tracking**: Know exactly where update stands
5. **Timeout Protection**: Won't wait forever
6. **Multiple Partition Support**: Safe A/B partition switching
7. **Resumable Downloads**: Can resume interrupted downloads

---

## Performance

### 16KB Chunks (Current Configuration)

- **Firmware Size**: 1040KB
- **Total Chunks**: 66
- **Download Time**: ~2 minutes (2 seconds per chunk)
- **Network Overhead**: Minimal
- **Memory Usage**: ~50KB peak (32KB buffer + overhead)

### Comparison with 1KB Chunks

| Metric | 1KB Chunks | 16KB Chunks | Improvement |
|--------|-----------|-------------|-------------|
| Total Chunks | 1041 | 66 | 15.8× fewer |
| Download Time | 35 min | 2.2 min | 15.9× faster |
| HTTP Requests | 1041 | 66 | 15.8× fewer |
| Server Load | High | Low | Much better |

---

## Summary

The FOTA Update Manager provides a complete, safe, and efficient firmware update system with:

✅ **Easy to use**: Interactive menu or programmatic API  
✅ **Safe**: Automatic verification and rollback  
✅ **Fast**: 16KB chunks = 16× faster than 1KB  
✅ **Reliable**: Resumable downloads, error handling  
✅ **Complete**: Upload, trigger, monitor, verify, rollback  

Perfect for production IoT deployments! 🚀
