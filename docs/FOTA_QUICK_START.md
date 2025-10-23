# FOTA Testing - Quick Start Guide

## 🚀 **Start Testing in 5 Minutes**

### **Prerequisites:**
- ✅ Flask server running: `python app.py`
- ✅ ESP32 connected and running
- ✅ Firmware built: `pio run`

---

## ⚡ **Option 1: Automated Complete Test (Recommended)**

**Run the automated test script:**
```powershell
.\test_fota_complete.ps1
```

**What it does:**
1. Uploads firmware to cloud
2. Monitors download progress with live progress bar
3. Waits for verification and reboot
4. Checks boot status
5. Reports PASS/FAIL

**Expected output:**
```
═══════════════════════════════════════════════════════
          Complete FOTA Test Script
═══════════════════════════════════════════════════════

[1/7] Checking firmware file...
✓ Found firmware: 876544 bytes (~856 chunks)

[2/7] Encoding firmware to Base64...
✓ Encoded: 1168728 characters

[3/7] Calculating SHA-256 hash...
✓ Hash: 7a8f3e2b1c5d9a4e6f0b8c2a1d3e5f7...

[4/7] Uploading firmware to cloud...
✓ Uploaded successfully
  Version: 1.0.1-test
  Total chunks: 856
  Hash: 7a8f3e2b1c5d9a4e6f0b8c2a1d3e5f7...

[5/7] Monitoring download progress...
  Download started!
  [5s] [████████░░░░░░░░░░░░░░░░░░░░] 10.0% (Chunk 86/856)
  [10s] [████████████████░░░░░░░░░░░░] 20.0% (Chunk 171/856)
  ...
  [180s] [████████████████████████████] 100.0% (Chunk 856/856)

✓ Download complete and verified!

[6/7] Waiting for device to apply update and reboot...
✓ Device should have rebooted

[7/7] Checking boot status...

✓ Device booted successfully with new firmware!

═══════════════════════════════════════════════════════
          ✓ FOTA TEST PASSED!
═══════════════════════════════════════════════════════
```

**Time:** ~5-15 minutes depending on firmware size

---

## ⚡ **Option 2: Monitor Progress Only**

**If you already uploaded firmware, just monitor progress:**
```powershell
.\monitor_fota_progress.ps1
```

**Expected output:**
```
═══════════════════════════════════════════════════════
          FOTA Progress Monitor (45s)
═══════════════════════════════════════════════════════

Firmware Manifest:
  Version:     1.0.1
  Size:        876544 bytes
  Hash:        7a8f3e2b1c5d9a4e6f0b8c2a1d3e5f7...
  Chunks:      856
  Chunk Size:  1024 bytes

Device Status (EcoWatt001):
  Chunks:      256 / 856
  Progress:    29.9%
  [████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░]
  Speed:       8.5 chunks/s
  ETA:         71s
  Verified:    ○ NO
  Last Update: 2025-10-18T14:30:45

═══════════════════════════════════════════════════════
Next refresh in 5 seconds...
```

**Refreshes automatically every 5 seconds**

---

## ⚡ **Option 3: Manual Testing via Web Dashboard**

### **Step 1: Open FOTA Dashboard**
```powershell
start http://10.52.180.183:8080/fota
```

### **Step 2: Upload Firmware**
1. Click "Choose File" → Select `.pio\build\esp32dev\firmware.bin`
2. Enter version: `1.0.1`
3. Click "Upload Firmware"
4. Wait for "Firmware uploaded successfully"

### **Step 3: Monitor in Browser**
- Dashboard shows device update status
- Refreshes automatically
- Shows chunks received, progress, verification status

### **Step 4: Check Serial Monitor**
```powershell
pio device monitor --port COM5 --baud 115200
```

Watch for:
```
[FOTA] Checking for firmware updates
[FOTA] New firmware available: version=1.0.1
[FOTA] Downloading chunk 0/856
[FOTA] Progress: 1/856 chunks (0.1%)
...
[FOTA] All chunks downloaded
[FOTA] Verifying firmware integrity
[FOTA] Firmware verified successfully
[FOTA] Applying firmware update
[FOTA] Rebooting in 3 seconds...
```

---

## 🧪 **Quick Individual Feature Tests**

### **Test 1: Resume After Interruption**
```powershell
# 1. Start test
.\test_fota_complete.ps1

# 2. When progress reaches ~30%, UNPLUG ESP32
# 3. Wait 5 seconds
# 4. Plug back in ESP32

# Expected: Download resumes from chunk 30, not from 0
```

### **Test 2: Check FOTA Logs**
```powershell
# View all FOTA logs
(Invoke-WebRequest -Uri "http://10.52.180.183:8080/api/cloud/logs/fota").Content | ConvertFrom-Json | Select-Object -ExpandProperty logs | Select-Object -Last 20 | Format-Table
```

### **Test 3: Check Command History (for comparison)**
```powershell
# View command history
(Invoke-WebRequest -Uri "http://10.52.180.183:8080/api/cloud/command/history?device_id=EcoWatt001").Content | ConvertFrom-Json | Select-Object -ExpandProperty history | Format-Table
```

---

## 📊 **What to Look For (Success Indicators)**

### **ESP32 Serial Output:**
✅ `[FOTA] Checking for firmware updates`  
✅ `[FOTA] New firmware available`  
✅ `[FOTA] Downloading chunk X/Y`  
✅ `[FOTA] Chunk X HMAC verified successfully`  
✅ `[FOTA] Progress: X/Y chunks (Z%)`  
✅ `[FOTA] All chunks downloaded`  
✅ `[FOTA] Verifying firmware integrity`  
✅ `[FOTA] Firmware verification successful`  
✅ `[FOTA] Writing firmware to OTA partition`  
✅ `[FOTA] Firmware written successfully`  
✅ `[FOTA] Rebooting in 3 seconds...`  
✅ (After reboot) `[FOTA] Reporting boot status`  
✅ `[FOTA] Boot successful`

### **Flask Server Console:**
✅ `[FOTA] Firmware uploaded`  
✅ `[FOTA STATUS] Device EcoWatt001: chunk X, verified=False`  
✅ `[FOTA STATUS] Device EcoWatt001: chunk Y, verified=True`  
✅ `[FOTA STATUS] Device EcoWatt001: boot=success`

### **Cloud Status API:**
```powershell
# Should show:
{
  "device_status": {
    "EcoWatt001": {
      "chunk_received": 856,
      "verified": true,
      "boot_status": "success",
      "last_update": "2025-10-18T..."
    }
  }
}
```

---

## ❌ **Common Issues & Solutions**

### **Issue 1: Firmware file not found**
```
✗ Firmware file not found: .pio\build\esp32dev\firmware.bin
```
**Solution:**
```powershell
pio run
```

### **Issue 2: Device not downloading**
```
Waiting for device to start download...
..........
```
**Check:**
1. Is ESP32 connected to WiFi? Check serial monitor
2. Is Flask server running? Check `http://10.52.180.183:8080/fota`
3. Did firmware upload succeed? Check Flask console

### **Issue 3: Hash verification fails**
```
[FOTA] Hash mismatch: expected abc123..., got def456...
```
**Cause:** Firmware corrupted during download or wrong firmware uploaded  
**Solution:** Re-upload firmware, ensure file is not corrupted

### **Issue 4: Device stuck at verification**
```
[FOTA] Verifying firmware integrity
... (hangs)
```
**Cause:** Firmware file may be corrupted or too large for flash  
**Solution:** Check LittleFS has enough space, check serial for errors

### **Issue 5: Boot status not reported**
```
Waiting for boot status... (timeout)
```
**Check:**
1. Did device reboot? Check power LED
2. Is device connected to WiFi after reboot?
3. Check serial monitor for boot messages
4. Check Flask server logs

---

## 🎯 **Testing Checklist**

**Before starting:**
- [ ] Flask server running (`python app.py`)
- [ ] ESP32 connected and powered
- [ ] Firmware built (`pio run`)
- [ ] WiFi credentials correct

**During test:**
- [ ] Firmware uploads successfully
- [ ] Chunks download progressively
- [ ] HMAC verified per chunk
- [ ] Progress reported every 5 seconds
- [ ] Hash verification succeeds
- [ ] OTA write completes
- [ ] Device reboots

**After test:**
- [ ] Device boots new firmware
- [ ] Boot status "success" reported
- [ ] All logs present on cloud
- [ ] No errors in serial output

---

## 📝 **Quick Commands Reference**

```powershell
# Build firmware
pio run

# Upload firmware (normal upload, not FOTA)
pio run --target upload --upload-port COM5

# Start Flask server
python app.py

# Run complete FOTA test
.\test_fota_complete.ps1

# Monitor FOTA progress
.\monitor_fota_progress.ps1

# Serial monitor
pio device monitor --port COM5 --baud 115200

# Check FOTA status
(Invoke-WebRequest -Uri "http://10.52.180.183:8080/api/cloud/fota/status").Content | ConvertFrom-Json

# Check FOTA logs
(Invoke-WebRequest -Uri "http://10.52.180.183:8080/api/cloud/logs/fota").Content | ConvertFrom-Json

# Open FOTA dashboard
start http://10.52.180.183:8080/fota
```

---

## 🎉 **Expected Test Result**

If everything works correctly:

```
═══════════════════════════════════════════════════════
          ✓ FOTA TEST PASSED!
═══════════════════════════════════════════════════════

Summary:
  Firmware size:   876544 bytes
  Total chunks:    856
  Total time:      12.3 minutes
  Final status:    SUCCESS

All FOTA features verified:
  ✓ Chunked download with resume support
  ✓ SHA-256 integrity verification
  ✓ HMAC authenticity verification per chunk
  ✓ Automatic rollback on failure
  ✓ Controlled reboot after verification
  ✓ Boot status reporting
  ✓ FOTA loop integration
  ✓ Progress and status reporting
  ✓ Complete FOTA logging
```

---

## 📚 **Need More Details?**

See complete testing documentation:
- `FOTA_TESTING_COMPLETE_GUIDE.md` - Detailed step-by-step guide
- `PART4_FOTA_CHECKLIST.md` - Implementation verification
- `MILESTONE4_COMPLETE_VERIFICATION.md` - Overall project status

---

**Ready? Run the test now!** 🚀
```powershell
.\test_fota_complete.ps1
```
