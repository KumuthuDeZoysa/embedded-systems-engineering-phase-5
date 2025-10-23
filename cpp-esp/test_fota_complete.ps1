# Complete FOTA Test Script with Debug Output
param(
    [string]$FirmwarePath = ".pio\build\esp32dev\firmware.bin",
    [string]$Version = "1.0.1-test",
    [string]$ServerUrl = "http://10.52.180.183:8080",
    [string]$DeviceId = "EcoWatt001",
    [int]$ChunkSize = 1024
)

$DebugPreference = "Continue"

Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "       Complete FOTA Test Script (DEBUG MODE)" -ForegroundColor Yellow
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host ""
Write-Debug "Server URL: $ServerUrl"
Write-Debug "Device ID: $DeviceId"
Write-Debug "Firmware: $FirmwarePath"
Write-Host ""

# Step 1: Check firmware
Write-Host "[1/7] Checking firmware file..." -ForegroundColor Yellow
Write-Debug "Checking path: $FirmwarePath"
if (-not (Test-Path $FirmwarePath)) {
    Write-Host "[ERROR] Firmware not found: $FirmwarePath" -ForegroundColor Red
    Write-Host "Build first: pio run" -ForegroundColor Yellow
    exit 1
}
$fileSize = (Get-Item $FirmwarePath).Length
$totalChunks = [math]::Ceiling($fileSize / $ChunkSize)
Write-Host "[OK] Found: $fileSize bytes (~$totalChunks chunks)" -ForegroundColor Green
Write-Debug "Chunk size: $ChunkSize, Total chunks: $totalChunks"
Write-Host ""

# Step 2: Encode
Write-Host "[2/7] Encoding to Base64..." -ForegroundColor Yellow
Write-Debug "Reading firmware bytes..."
$firmwareBytes = [System.IO.File]::ReadAllBytes($FirmwarePath)
Write-Debug "Converting to Base64..."
$base64 = [System.Convert]::ToBase64String($firmwareBytes)
Write-Host "[OK] Encoded: $($base64.Length) chars" -ForegroundColor Green
Write-Debug "Base64 preview: $($base64.Substring(0, [Math]::Min(50, $base64.Length)))..."
Write-Host ""

# Step 3: Hash
Write-Host "[3/7] Calculating SHA-256..." -ForegroundColor Yellow
Write-Debug "Computing hash..."
$sha256 = [System.Security.Cryptography.SHA256]::Create()
$hashBytes = $sha256.ComputeHash($firmwareBytes)
$hash = ($hashBytes | ForEach-Object { $_.ToString("x2") }) -join ''
Write-Host "[OK] Hash: $hash" -ForegroundColor Green
Write-Host ""

# Step 4: Upload
Write-Host "[4/7] Uploading to cloud..." -ForegroundColor Yellow
$payload = @{
    version = $Version
    size = $firmwareBytes.Length
    hash = $hash
    chunk_size = $ChunkSize
    firmware_data = $base64
} | ConvertTo-Json

Write-Debug "Payload size: $($payload.Length) bytes"
Write-Debug "POST to: $ServerUrl/api/cloud/fota/upload"

try {
    $response = Invoke-WebRequest -Uri "$ServerUrl/api/cloud/fota/upload" -Method Post -Body $payload -ContentType "application/json" -TimeoutSec 60
    $result = $response.Content | ConvertFrom-Json
    Write-Host "[OK] Uploaded: $($result.total_chunks) chunks" -ForegroundColor Green
    Write-Debug "Server response: $($response.Content)"
} catch {
    Write-Host "[ERROR] Upload failed: $_" -ForegroundColor Red
    Write-Debug "Error details: $($_.Exception.Message)"
    exit 1
}
Write-Host ""

# Step 5: Monitor
Write-Host "[5/7] Monitoring download..." -ForegroundColor Yellow
Write-Host "Waiting for device (ESP32 polls every 60s)..." -ForegroundColor Gray
Write-Debug "Starting download monitor..."
Write-Host ""

$lastChunk = -1
$startTime = Get-Date
$started = $false
$pollCount = 0

while ($true) {
    try {
        $pollCount++
        Write-Debug "Poll #$pollCount - Fetching status..."
        
        $status = (Invoke-WebRequest -Uri "$ServerUrl/api/cloud/fota/status").Content | ConvertFrom-Json
        $device = $status.device_status.$DeviceId
        
        if ($device -and $device.chunk_received -ne $null) {
            if (-not $started) {
                $started = $true
                Write-Host ""
                Write-Host "=====> Download started! <=====" -ForegroundColor Green
                Write-Host ""
                $startTime = Get-Date
            }
            
            if ($device.chunk_received -ne $lastChunk) {
                $lastChunk = $device.chunk_received
                $pct = if ($device.progress) { [math]::Round($device.progress, 1) } else { 0 }
                $elapsed = [math]::Round(((Get-Date) - $startTime).TotalSeconds, 0)
                
                $filled = [math]::Floor(40 * $pct / 100)
                $empty = 40 - $filled
                $bar = "[" + ("#" * $filled) + ("-" * $empty) + "]"
                
                Write-Host "[$elapsed`s] $bar $pct%" -ForegroundColor Cyan
                Write-Host "        Chunk $($device.chunk_received)/$($status.total_chunks)" -ForegroundColor Gray
                Write-Debug "Device status: chunk=$($device.chunk_received), verified=$($device.verified), error=$($device.error)"
                
                if ($device.verified) {
                    Write-Host ""
                    Write-Host "[OK] Download complete and verified!" -ForegroundColor Green
                    break
                }
            }
        } else {
            if (-not $started) {
                if ($pollCount % 5 -eq 0) {
                    Write-Host "Waiting... (${pollCount} polls, device hasn't started download yet)" -ForegroundColor DarkGray
                } else {
                    Write-Host "." -NoNewline -ForegroundColor Gray
                }
            }
        }
        
        Start-Sleep -Seconds 2
        
    } catch {
        Write-Debug "Status check error: $_"
        Write-Host "x" -NoNewline -ForegroundColor Yellow
        Start-Sleep -Seconds 3
    }
    
    if (((Get-Date) - $startTime).TotalMinutes -gt 15) {
        Write-Host ""
        Write-Host "[ERROR] Timeout (15 min)" -ForegroundColor Red
        exit 1
    }
}
Write-Host ""

# Step 6: Wait for reboot
Write-Host "[6/7] Waiting for OTA write and reboot..." -ForegroundColor Yellow
Write-Host "Device is now:" -ForegroundColor Gray
Write-Host "  1. Writing firmware to OTA partition" -ForegroundColor Gray
Write-Host "  2. Calling esp_ota_set_boot_partition()" -ForegroundColor Gray
Write-Host "  3. Rebooting to new firmware" -ForegroundColor Gray
Write-Debug "Waiting 15 seconds for OTA process..."

for ($i = 15; $i -gt 0; $i--) {
    Write-Host "  Waiting... $i seconds" -ForegroundColor DarkGray
    Start-Sleep -Seconds 1
}

Write-Host "[OK] Device should have rebooted" -ForegroundColor Green
Write-Host ""

# Step 7: Boot status
Write-Host "[7/7] Checking boot status..." -ForegroundColor Yellow
Write-Host "Waiting for device to report boot status..." -ForegroundColor Gray
Write-Debug "Starting boot status check..."

$maxRetries = 20
$retry = 0

while ($retry -lt $maxRetries) {
    try {
        Write-Debug "Boot check attempt $($retry + 1)/$maxRetries"
        
        $status = (Invoke-WebRequest -Uri "$ServerUrl/api/cloud/fota/status").Content | ConvertFrom-Json
        $device = $status.device_status.$DeviceId
        
        Write-Debug "Boot status: $($device.boot_status)"
        
        if ($device.boot_status -eq "success") {
            Write-Host ""
            Write-Host "======================================================" -ForegroundColor Green
            Write-Host "       FOTA TEST PASSED!" -ForegroundColor Green
            Write-Host "======================================================" -ForegroundColor Green
            Write-Host ""
            Write-Host "Summary:" -ForegroundColor White
            Write-Host "  Version:    $Version" -ForegroundColor Gray
            Write-Host "  Size:       $fileSize bytes" -ForegroundColor Gray
            Write-Host "  Hash:       $hash" -ForegroundColor Gray
            Write-Host "  Chunks:     $totalChunks" -ForegroundColor Gray
            Write-Host "  Boot:       SUCCESS" -ForegroundColor Green
            Write-Host ""
            Write-Debug "Test completed successfully"
            exit 0
        } elseif ($device.boot_status -eq "failed") {
            Write-Host "[ERROR] Boot failed - device rolled back!" -ForegroundColor Red
            Write-Debug "Rollback triggered"
            exit 1
        }
        
        Write-Host "  Attempt $($retry + 1)/$maxRetries - No boot status yet..." -ForegroundColor DarkGray
        Start-Sleep -Seconds 5
        $retry++
        
    } catch {
        Write-Debug "Boot check error: $_"
        Write-Host "  Attempt $($retry + 1)/$maxRetries - Checking..." -ForegroundColor DarkGray
        Start-Sleep -Seconds 5
        $retry++
    }
}

Write-Host ""
Write-Host "[WARNING] Boot status timeout - check device manually" -ForegroundColor Yellow
Write-Debug "Boot status check timed out after $maxRetries attempts"
exit 1
