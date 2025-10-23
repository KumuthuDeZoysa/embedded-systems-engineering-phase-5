# Complete FOTA Testing Guide

**Date:** October 18, 2025  
**Purpose:** Test all FOTA features systematically

---

## 📋 **Pre-Test Setup**

### **1. Prepare Test Firmware**

You need a firmware binary to test with. You can either:

**Option A: Build Current Firmware**
```powershell
cd "E:\ES Phase 4\embedded-systems-engineering-phase-4\cpp-esp"
pio run
```

The `.bin` file will be at:
```
.pio\build\esp32dev\firmware.bin
```

**Option B: Create a Modified Version**
1. Make a small change (e.g., add a log message)
2. Update version in `src/config_manager.cpp`:
   ```cpp
   const char* version = "1.0.1-test";
   ```
3. Build: `pio run`

### **2. Start Flask Server**
```powershell
python app.py
```

Server should be running at: `http://10.52.180.183:8080`

### **3. Ensure ESP32 is Running**
- Upload current firmware: `pio run --target upload --upload-port COM5`
- ESP32 should be connected to WiFi and polling cloud

---

## 🧪 **TEST 1: Chunked Download with Resume Support**

### **Goal:** Verify firmware is downloaded in chunks and can resume after interruption

### **Steps:**

**1.1 Upload Firmware to Cloud**
```powershell
# Open FOTA dashboard in browser
start http://10.52.180.183:8080/fota
```

Or upload via API:
```powershell
# Read firmware file and encode to base64
$firmwarePath = ".pio\build\esp32dev\firmware.bin"
$firmwareBytes = [System.IO.File]::ReadAllBytes($firmwarePath)
$base64 = [System.Convert]::ToBase64String($firmwareBytes)

# Calculate hash
$sha256 = [System.Security.Cryptography.SHA256]::Create()
$hashBytes = $sha256.ComputeHash($firmwareBytes)
$hash = ($hashBytes | ForEach-Object { $_.ToString("x2") }) -join ''

# Prepare payload
$payload = @{
    version = "1.0.1"
    size = $firmwareBytes.Length
    hash = $hash
    chunk_size = 1024
    firmware_data = $base64
} | ConvertTo-Json

# Upload
Invoke-WebRequest -Uri "http://10.52.180.183:8080/api/cloud/fota/upload" `
    -Method Post `
    -Body $payload `
    -ContentType "application/json"
```

**1.2 Monitor Chunked Download**

Open serial monitor:
```powershell
pio device monitor --port COM5 --baud 115200
```

**Expected Output:**
```
[FOTA] Checking for firmware updates
[FOTA] New firmware available: version=1.0.1
[FOTA] Starting download: total_chunks=XXX
[FOTA] Downloading chunk 0/XXX
[FOTA] Chunk 0 HMAC verified successfully
[FOTA] Progress: 1/XXX chunks (0.X%)
[FOTA] Downloading chunk 1/XXX
[FOTA] Chunk 1 HMAC verified successfully
[FOTA] Progress: 2/XXX chunks (0.X%)
...
```

**1.3 Test Resume After Interruption**

While chunks are downloading:
1. **Unplug ESP32** (or press RESET button)
2. **Wait 5 seconds**
3. **Plug back in** (or release RESET)

**Expected Behavior:**
```
[FOTA] Initializing FOTA Manager
[FOTA] Loaded previous FOTA state: state=2, chunks=10/100
[FOTA] Resuming download from chunk 10
[FOTA] Downloading chunk 10/100
[FOTA] Progress: 11/100 chunks (11.0%)
...
```

✅ **Pass Criteria:**
- Chunks downloaded one by one (not all at once)
- Progress saved to `/fota_state.json`
- After interruption, download resumes from last successful chunk
- No duplicate chunk downloads

---

## 🧪 **TEST 2: SHA-256 Integrity Verification**

### **Goal:** Verify firmware integrity is checked before OTA write

### **Steps:**

**2.1 Let Download Complete**

Wait for all chunks to download:
```
[FOTA] Downloading chunk 98/100
[FOTA] Downloading chunk 99/100
[FOTA] All chunks downloaded, proceeding to verification
```

**2.2 Observe Hash Verification**

**Expected Output:**
```
[FOTA] Verifying firmware integrity
[FOTA] Firmware verification successful: hash=abc123...
[FOTA] Firmware verified successfully
```

✅ **Pass Criteria:**
- SHA-256 hash calculated from complete downloaded firmware
- Hash matches manifest hash from cloud
- Verification logged with hash value

**2.3 Test Hash Mismatch (Manual Test)**

To test hash verification failure:
1. **Modify Flask server** to send wrong hash:
   ```python
   # In app.py, upload_firmware function
   FIRMWARE_MANIFEST = {
       'hash': 'wrong_hash_for_testing',  # Wrong hash
       ...
   }
   ```
2. Upload firmware again
3. Let download complete

**Expected Output:**
```
[FOTA] Verifying firmware integrity
[FOTA] Hash mismatch: expected wrong_hash_for_testing, got abc123...
[FOTA] State changed to FAILED: Hash verification failed
```

✅ **Pass Criteria:**
- Verification fails with hash mismatch error
- State changes to FAILED
- No OTA write attempted

---

## 🧪 **TEST 3: HMAC Authenticity Verification Per Chunk**

### **Goal:** Verify each chunk is authenticated before saving

### **Steps:**

**3.1 Monitor Chunk HMAC Verification**

During download, watch for HMAC messages:

**Expected Output:**
```
[FOTA] Downloading chunk 5/100
[FOTA] Chunk 5 HMAC verified successfully
[FOTA] Chunk 5 saved to file
```

✅ **Pass Criteria:**
- Each chunk verified with HMAC before saving
- "HMAC verified successfully" message per chunk
- Invalid HMAC rejects chunk

**3.2 Test HMAC Failure (Manual Test)**

To test HMAC verification failure:
1. **Modify Flask server** to send wrong HMAC:
   ```python
   # In app.py, upload_firmware function
   FIRMWARE_CHUNKS[i] = {
       'chunk_number': i,
       'data': chunk_b64,
       'mac': 'invalid_hmac_for_testing',  # Wrong HMAC
       'size': len(chunk_data)
   }
   ```
2. Upload firmware again
3. Watch first chunk download

**Expected Output:**
```
[FOTA] Downloading chunk 0/100
[FOTA] HMAC verification failed for chunk 0
[FOTA] Failed to fetch chunk 0
[FOTA] Retrying chunk 0...
```

✅ **Pass Criteria:**
- HMAC verification fails
- Chunk rejected
- Download retries or fails

---

## 🧪 **TEST 4: Automatic Rollback on Failure**

### **Goal:** Verify automatic rollback on verification or boot failure

### **Test 4A: Rollback on Verification Failure**

**Steps:**
1. Upload firmware with wrong hash (see Test 2.3)
2. Let download complete
3. Observe verification failure

**Expected Output:**
```
[FOTA] Verifying firmware integrity
[FOTA] Hash mismatch: expected wrong_hash, got abc123...
[FOTA] State changed to FAILED: Firmware verification failed
```

✅ **Pass Criteria:**
- Verification fails
- No OTA write attempted
- Device remains on current firmware

### **Test 4B: Rollback on Boot Failure**

**This test requires corrupted firmware (advanced test):**

**Steps:**
1. Create intentionally broken firmware
2. Upload and let it install
3. Device will reboot with new firmware
4. New firmware fails to initialize
5. Device reboots again (boot count increments)
6. After 3 failed boots, automatic rollback triggers

**Expected Behavior:**
```
[Boot 1] Boot count: 1 (new firmware)
[Boot 1] Failed to initialize...
[Reboot]
[Boot 2] Boot count: 2 (new firmware)
[Boot 2] Failed to initialize...
[Reboot]
[Boot 3] Boot count: 3 (new firmware)
[Boot 3] Boot count exceeded (3), triggering rollback
[FOTA] Initiating rollback: Boot count exceeded
[FOTA] Rolling back to factory partition
[Reboot]
[Boot 4] Running from factory partition (old firmware)
```

✅ **Pass Criteria:**
- Boot count tracked across reboots
- After 3 failed boots, rollback triggered
- Device reverts to factory/previous OTA partition
- Rollback logged to cloud

**⚠️ Warning:** This test can brick your device if factory partition doesn't exist. **Skip if unsure.**

---

## 🧪 **TEST 5: Controlled Reboot After Verification**

### **Goal:** Verify reboot only happens after successful verification

### **Steps:**

**5.1 Complete Download and Verification**

Let firmware download and verify successfully:

**Expected Output:**
```
[FOTA] All chunks downloaded
[FOTA] Verifying firmware integrity
[FOTA] Firmware verification successful: hash=abc123...
[FOTA] Firmware verified successfully, applying update...
[FOTA] Applying firmware update
[FOTA] Writing firmware to OTA partition: XXXXX bytes
[FOTA] Written: 10240 / XXXXX bytes (5%)
[FOTA] Written: 20480 / XXXXX bytes (10%)
...
[FOTA] Firmware written successfully: XXXXX bytes
[FOTA] Rebooting in 3 seconds...
[Reboot]
```

✅ **Pass Criteria:**
- Reboot ONLY after successful hash verification
- Reboot ONLY after successful OTA write
- 3-second delay before reboot (for logging)
- No premature reboots

---

## 🧪 **TEST 6: Boot Status Reporting**

### **Goal:** Verify boot status is reported to cloud after reboot

### **Steps:**

**6.1 Monitor Serial After Reboot**

After FOTA update reboots device:

**Expected Output:**
```
[Boot] ESP32 starting...
[WiFi] Connecting to WiFi...
[WiFi] Connected: IP=XXX.XXX.XXX.XXX
[Device] FOTA Manager initialized
[FOTA] Reporting boot status
[FOTA] Boot successful, firmware update completed
[HTTP] POST /api/inverter/fota/status - Status: 200
```

**6.2 Check Flask Server Logs**

Flask console should show:
```
[FOTA STATUS] Device EcoWatt001: boot=success
```

**6.3 Check Cloud Status**

Query FOTA status:
```powershell
(Invoke-WebRequest -Uri "http://10.52.180.183:8080/api/cloud/fota/status").Content | ConvertFrom-Json
```

**Expected Response:**
```json
{
  "device_status": {
    "EcoWatt001": {
      "boot_status": "success",
      "last_update": "2025-10-18T..."
    }
  }
}
```

✅ **Pass Criteria:**
- `reportBootStatus()` called on startup
- Boot status sent to cloud
- Cloud receives and logs boot status
- Boot status = "success" for successful boot

---

## 🧪 **TEST 7: FOTA Loop Integration**

### **Goal:** Verify `fota_->loop()` is called and chunks download automatically

### **Steps:**

**7.1 Upload Firmware**

Upload firmware to cloud (see Test 1.1)

**7.2 Do NOT Monitor Serial**

Let device run normally without serial monitor

**7.3 Check Progress After 2-3 Minutes**

Query FOTA status:
```powershell
(Invoke-WebRequest -Uri "http://10.52.180.183:8080/api/cloud/fota/status").Content | ConvertFrom-Json
```

**Expected Response:**
```json
{
  "device_status": {
    "EcoWatt001": {
      "chunk_received": 45,
      "verified": false,
      "progress": 45.0,
      "last_update": "2025-10-18T..."
    }
  }
}
```

✅ **Pass Criteria:**
- Chunks download automatically (without manual intervention)
- `chunk_received` count increases over time
- `fota_->loop()` processing chunks in background
- No user interaction needed

---

## 🧪 **TEST 8: Progress and Status Reporting**

### **Goal:** Verify progress is reported to cloud during download

### **Steps:**

**8.1 Start Firmware Download**

Upload firmware and start download (see Test 1.1)

**8.2 Monitor Cloud Status in Real-Time**

Run this PowerShell script to poll status every 5 seconds:

```powershell
# Save as monitor_fota_progress.ps1
while ($true) {
    Clear-Host
    $status = (Invoke-WebRequest -Uri "http://10.52.180.183:8080/api/cloud/fota/status").Content | ConvertFrom-Json
    
    $device = $status.device_status.EcoWatt001
    if ($device) {
        Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "     FOTA Progress Monitor" -ForegroundColor Yellow
        Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Chunks:    " -NoNewline
        Write-Host "$($device.chunk_received) / $($status.total_chunks)" -ForegroundColor Green
        Write-Host "Progress:  " -NoNewline
        Write-Host "$([math]::Round($device.progress, 1))%" -ForegroundColor Green
        Write-Host "Verified:  " -NoNewline
        Write-Host "$($device.verified)" -ForegroundColor $(if ($device.verified) { "Green" } else { "Yellow" })
        Write-Host "Updated:   " -NoNewline
        Write-Host "$($device.last_update)" -ForegroundColor Gray
        
        if ($device.error) {
            Write-Host ""
            Write-Host "Error:     " -NoNewline
            Write-Host "$($device.error)" -ForegroundColor Red
        }
    } else {
        Write-Host "No FOTA status available" -ForegroundColor Yellow
    }
    
    Start-Sleep -Seconds 5
}
```

Run it:
```powershell
.\monitor_fota_progress.ps1
```

**Expected Output:**
```
═══════════════════════════════════════
     FOTA Progress Monitor
═══════════════════════════════════════

Chunks:    15 / 100
Progress:  15.0%
Verified:  False
Updated:   2025-10-18T14:30:15

[After 5 seconds]

Chunks:    20 / 100
Progress:  20.0%
Verified:  False
Updated:   2025-10-18T14:30:20

[Continue until 100%]

Chunks:    100 / 100
Progress:  100.0%
Verified:  True
Updated:   2025-10-18T14:31:05
```

✅ **Pass Criteria:**
- Progress reported every 5 seconds (configurable)
- Chunk count increases progressively
- Progress percentage calculated correctly
- `verified` changes to `true` after completion
- Status updates timestamp on each report

**8.3 Check Flask Logs**

Flask console should show:
```
[FOTA STATUS] Device EcoWatt001: chunk 15, verified=False, boot=None, rollback=False
[FOTA STATUS] Device EcoWatt001: chunk 20, verified=False, boot=None, rollback=False
...
[FOTA STATUS] Device EcoWatt001: chunk 100, verified=True, boot=None, rollback=False
```

---

## 🧪 **TEST 9: Complete FOTA Logging**

### **Goal:** Verify all FOTA events are logged on cloud

### **Steps:**

**9.1 Complete Full FOTA Cycle**

1. Upload firmware
2. Let download complete
3. Let verification pass
4. Let device reboot
5. Wait for boot status report

**9.2 Query FOTA Logs**

```powershell
# Get all FOTA logs
$logs = (Invoke-WebRequest -Uri "http://10.52.180.183:8080/api/cloud/logs/fota").Content | ConvertFrom-Json

# Display formatted
$logs.logs | Select-Object -Last 20 | Format-Table timestamp, device_id, event_type, details -AutoSize
```

**Expected Log Events (in order):**
```
timestamp              device_id   event_type              details
---------              ---------   ----------              -------
2025-10-18T14:25:00    cloud       firmware_uploaded       Version: 1.0.1, Size: 123456 bytes...
2025-10-18T14:25:00    cloud       firmware_decoded        Size: 123456 bytes
2025-10-18T14:25:00    cloud       firmware_hash_verified  Hash: abc123...
2025-10-18T14:25:00    cloud       chunking_started        Creating 121 chunks of 1024 bytes
2025-10-18T14:25:30    EcoWatt001  chunk_received          Chunk 0/121 (0.8%)
2025-10-18T14:25:35    EcoWatt001  chunk_received          Chunk 5/121 (4.1%)
...
2025-10-18T14:30:00    EcoWatt001  chunk_received          Chunk 120/121 (99.2%)
2025-10-18T14:30:05    EcoWatt001  firmware_verified       Hash verification successful
2025-10-18T14:30:10    EcoWatt001  firmware_applied        Version: 1.0.1, Size: 123456
2025-10-18T14:30:15    EcoWatt001  boot_status             Status: pending_reboot
2025-10-18T14:30:30    EcoWatt001  boot_status             Status: success
2025-10-18T14:30:30    EcoWatt001  boot_successful         Version: 1.0.1
2025-10-18T14:30:30    EcoWatt001  fota_completed          New firmware booted successfully
```

**9.3 Check for Specific Events**

Query with filters:
```powershell
# Get only chunk progress events
$logs = (Invoke-WebRequest -Uri "http://10.52.180.183:8080/api/cloud/logs/fota?device_id=EcoWatt001").Content | ConvertFrom-Json
$logs.logs | Where-Object { $_.event_type -eq "chunk_received" } | Select-Object -Last 10
```

✅ **Pass Criteria:**
- All FOTA events logged with timestamps
- Logs include: upload, decode, hash verify, chunking, chunk progress, firmware verify, apply, boot status, completion
- Logs filterable by device_id
- Logs include detailed information (sizes, hashes, progress)

---

## 📊 **Complete Test Checklist**

Run through all tests and mark completion:

### **Feature Testing:**
- [ ] **Test 1:** Chunked download works
- [ ] **Test 1:** Resume after interruption works
- [ ] **Test 2:** SHA-256 integrity verification works
- [ ] **Test 2:** Hash mismatch detected
- [ ] **Test 3:** HMAC per-chunk verification works
- [ ] **Test 3:** Invalid HMAC rejected
- [ ] **Test 4A:** Rollback on verification failure works
- [ ] **Test 4B:** Rollback on boot failure works (optional)
- [ ] **Test 5:** Controlled reboot after verification
- [ ] **Test 6:** Boot status reported to cloud
- [ ] **Test 7:** Automatic chunk download (loop integration)
- [ ] **Test 8:** Progress reporting every 5 seconds
- [ ] **Test 9:** Complete FOTA event logging

### **Integration Testing:**
- [ ] Full FOTA cycle completes successfully
- [ ] Device boots new firmware successfully
- [ ] Cloud receives all status updates
- [ ] Logs contain complete FOTA lifecycle

---

## 🎯 **Quick Test Script (Full FOTA Cycle)**

Save this as `test_fota_complete.ps1`:

```powershell
# Complete FOTA Test Script
param(
    [string]$FirmwarePath = ".pio\build\esp32dev\firmware.bin",
    [string]$Version = "1.0.1-test",
    [string]$ServerUrl = "http://10.52.180.183:8080",
    [string]$DeviceId = "EcoWatt001"
)

Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Complete FOTA Test Script" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Step 1: Check firmware file exists
Write-Host "[1/7] Checking firmware file..." -ForegroundColor Yellow
if (-not (Test-Path $FirmwarePath)) {
    Write-Host "✗ Firmware file not found: $FirmwarePath" -ForegroundColor Red
    Write-Host "Build firmware first: pio run" -ForegroundColor Yellow
    exit 1
}
$fileSize = (Get-Item $FirmwarePath).Length
Write-Host "✓ Found firmware: $fileSize bytes" -ForegroundColor Green
Write-Host ""

# Step 2: Encode firmware
Write-Host "[2/7] Encoding firmware to Base64..." -ForegroundColor Yellow
$firmwareBytes = [System.IO.File]::ReadAllBytes($FirmwarePath)
$base64 = [System.Convert]::ToBase64String($firmwareBytes)
Write-Host "✓ Encoded: $($base64.Length) characters" -ForegroundColor Green
Write-Host ""

# Step 3: Calculate hash
Write-Host "[3/7] Calculating SHA-256 hash..." -ForegroundColor Yellow
$sha256 = [System.Security.Cryptography.SHA256]::Create()
$hashBytes = $sha256.ComputeHash($firmwareBytes)
$hash = ($hashBytes | ForEach-Object { $_.ToString("x2") }) -join ''
Write-Host "✓ Hash: $hash" -ForegroundColor Green
Write-Host ""

# Step 4: Upload firmware
Write-Host "[4/7] Uploading firmware to cloud..." -ForegroundColor Yellow
$payload = @{
    version = $Version
    size = $firmwareBytes.Length
    hash = $hash
    chunk_size = 1024
    firmware_data = $base64
} | ConvertTo-Json

try {
    $response = Invoke-WebRequest -Uri "$ServerUrl/api/cloud/fota/upload" `
        -Method Post `
        -Body $payload `
        -ContentType "application/json"
    
    $result = $response.Content | ConvertFrom-Json
    Write-Host "✓ Uploaded: Version $($result.manifest.version), $($result.total_chunks) chunks" -ForegroundColor Green
} catch {
    Write-Host "✗ Upload failed: $_" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Step 5: Monitor progress
Write-Host "[5/7] Monitoring download progress..." -ForegroundColor Yellow
Write-Host "Press Ctrl+C to stop monitoring" -ForegroundColor Gray
Write-Host ""

$lastChunk = -1
$startTime = Get-Date

while ($true) {
    try {
        $status = (Invoke-WebRequest -Uri "$ServerUrl/api/cloud/fota/status").Content | ConvertFrom-Json
        $device = $status.device_status.$DeviceId
        
        if ($device -and $device.chunk_received -ne $lastChunk) {
            $lastChunk = $device.chunk_received
            $progress = [math]::Round($device.progress, 1)
            $elapsed = [math]::Round(((Get-Date) - $startTime).TotalSeconds, 0)
            
            Write-Host "  [$elapsed`s] Chunk $($device.chunk_received)/$($status.total_chunks) ($progress%)" -ForegroundColor Cyan
            
            if ($device.verified) {
                Write-Host ""
                Write-Host "✓ Download complete, firmware verified!" -ForegroundColor Green
                break
            }
        }
        
        Start-Sleep -Seconds 2
        
    } catch {
        Write-Host "  Waiting for device..." -ForegroundColor Gray
        Start-Sleep -Seconds 5
    }
}
Write-Host ""

# Step 6: Wait for reboot
Write-Host "[6/7] Waiting for device reboot..." -ForegroundColor Yellow
Start-Sleep -Seconds 10

# Step 7: Check boot status
Write-Host "[7/7] Checking boot status..." -ForegroundColor Yellow
$maxRetries = 10
$retry = 0

while ($retry -lt $maxRetries) {
    try {
        $status = (Invoke-WebRequest -Uri "$ServerUrl/api/cloud/fota/status").Content | ConvertFrom-Json
        $device = $status.device_status.$DeviceId
        
        if ($device.boot_status -eq "success") {
            Write-Host "✓ Device booted successfully with new firmware!" -ForegroundColor Green
            Write-Host ""
            Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
            Write-Host "  ✓ FOTA TEST PASSED" -ForegroundColor Green
            Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
            exit 0
        } elseif ($device.boot_status -eq "failed") {
            Write-Host "✗ Device boot failed!" -ForegroundColor Red
            exit 1
        }
        
        Write-Host "  Waiting for boot status... ($retry/$maxRetries)" -ForegroundColor Gray
        Start-Sleep -Seconds 5
        $retry++
        
    } catch {
        Write-Host "  Waiting for device... ($retry/$maxRetries)" -ForegroundColor Gray
        Start-Sleep -Seconds 5
        $retry++
    }
}

Write-Host "✗ Boot status not received within timeout" -ForegroundColor Red
exit 1
```

**Run complete test:**
```powershell
.\test_fota_complete.ps1
```

---

## 📝 **Expected Results Summary**

### **Successful FOTA Cycle:**
1. ✅ Firmware uploaded and chunked on cloud
2. ✅ Device downloads chunks one by one
3. ✅ Each chunk HMAC verified before saving
4. ✅ Progress reported every 5 seconds
5. ✅ Complete download → SHA-256 hash verified
6. ✅ Firmware written to OTA partition
7. ✅ 3-second delay → controlled reboot
8. ✅ Device boots new firmware
9. ✅ Boot status "success" reported to cloud
10. ✅ All events logged on cloud

### **Total Test Time:**
- Small firmware (~500KB): 5-10 minutes
- Large firmware (~1MB): 10-20 minutes
- Depends on chunk size and polling interval

---

## 🎯 **Success Criteria**

All tests must pass:
- ✅ Chunked download works
- ✅ Resume after interruption works
- ✅ SHA-256 verification works
- ✅ HMAC per-chunk verification works
- ✅ Rollback on failure works
- ✅ Controlled reboot works
- ✅ Boot status reporting works
- ✅ FOTA loop integration works
- ✅ Progress reporting works
- ✅ Complete logging works

**If all pass: FOTA implementation is 100% functional!** 🎉

---

**Ready to start testing?** Begin with Test 1! 🚀
