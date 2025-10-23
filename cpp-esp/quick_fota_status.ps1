# Quick FOTA Status Checker
param(
    [string]$ServerUrl = "http://10.52.180.183:8080",
    [string]$DeviceId = "EcoWatt001",
    [int]$Checks = 20
)

Write-Host "Checking FOTA status $Checks times..." -ForegroundColor Cyan
Write-Host ""

for ($i = 1; $i -le $Checks; $i++) {
    try {
        $status = (Invoke-WebRequest -Uri "$ServerUrl/api/cloud/fota/status").Content | ConvertFrom-Json
        $device = $status.device_status.$DeviceId
        
        $timestamp = Get-Date -Format "HH:mm:ss"
        
        if ($device -and $device.chunk_received -ne $null) {
            $pct = if ($device.progress) { [math]::Round($device.progress, 1) } else { 0 }
            Write-Host "[$timestamp] Chunk $($device.chunk_received)/$($status.total_chunks) ($pct%) - Verified: $($device.verified)" -ForegroundColor Green
        } else {
            Write-Host "[$timestamp] Waiting for download to start..." -ForegroundColor Yellow
        }
        
    } catch {
        Write-Host "[$timestamp] Error: $_" -ForegroundColor Red
    }
    
    if ($i -lt $Checks) {
        Start-Sleep -Seconds 3
    }
}

Write-Host ""
Write-Host "Done!" -ForegroundColor Cyan
