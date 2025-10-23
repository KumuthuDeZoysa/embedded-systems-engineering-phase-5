# FOTA Progress Monitor
param(
    [string]$ServerUrl = "http://10.52.180.183:8080",
    [string]$DeviceId = "EcoWatt001",
    [int]$RefreshSeconds = 5
)

Write-Host "FOTA Progress Monitor" -ForegroundColor Cyan
Write-Host "Server: $ServerUrl" -ForegroundColor Gray
Write-Host "Device: $DeviceId" -ForegroundColor Gray
Write-Host "Press Ctrl+C to stop" -ForegroundColor Yellow
Write-Host ""

$startTime = Get-Date

while ($true) {
    Clear-Host
    
    $elapsed = [math]::Round(((Get-Date) - $startTime).TotalSeconds, 0)
    
    Write-Host "======================================================" -ForegroundColor Cyan
    Write-Host "     FOTA Progress Monitor ($elapsed`s)" -ForegroundColor Yellow
    Write-Host "======================================================" -ForegroundColor Cyan
    Write-Host ""
    
    try {
        $response = Invoke-WebRequest -Uri "$ServerUrl/api/cloud/fota/status"
        $status = $response.Content | ConvertFrom-Json
        
        if ($status.manifest) {
            Write-Host "Firmware Manifest:" -ForegroundColor White
            Write-Host "  Version:     $($status.manifest.version)" -ForegroundColor Green
            Write-Host "  Size:        $($status.manifest.size) bytes" -ForegroundColor Green
            Write-Host "  Hash:        $($status.manifest.hash.Substring(0,32))..." -ForegroundColor Gray
            Write-Host "  Chunks:      $($status.total_chunks)" -ForegroundColor Green
            Write-Host "  Chunk Size:  $($status.manifest.chunk_size) bytes" -ForegroundColor Green
            Write-Host ""
        } else {
            Write-Host "No firmware uploaded" -ForegroundColor Yellow
            Write-Host ""
        }
        
        $device = $status.device_status.$DeviceId
        if ($device) {
            Write-Host "Device Status ($DeviceId):" -ForegroundColor White
            
            if ($device.chunk_received -ne $null) {
                $pct = if ($device.progress) { [math]::Round($device.progress, 1) } else { 0 }
                $chunks = "$($device.chunk_received) / $($status.total_chunks)"
                
                Write-Host "  Chunks:      $chunks" -ForegroundColor Green
                Write-Host "  Progress:    $pct%" -ForegroundColor Green
                
                $filled = [math]::Floor(40 * $pct / 100)
                $empty = 40 - $filled
                $bar = "[" + ("#" * $filled) + ("-" * $empty) + "]"
                Write-Host "  $bar" -ForegroundColor Cyan
            }
            
            Write-Host "  Verified:    $(if ($device.verified) { 'YES' } else { 'NO' })" -ForegroundColor $(if ($device.verified) { "Green" } else { "Yellow" })
            
            if ($device.boot_status) {
                Write-Host "  Boot:        $($device.boot_status)" -ForegroundColor $(if ($device.boot_status -eq "success") { "Green" } else { "Yellow" })
            }
            
            if ($device.rollback) {
                Write-Host "  ROLLBACK:    TRIGGERED" -ForegroundColor Red
            }
            
            if ($device.error) {
                Write-Host "  Error:       $($device.error)" -ForegroundColor Red
            }
            
            Write-Host "  Updated:     $($device.last_update)" -ForegroundColor Gray
            
        } else {
            Write-Host "No device status" -ForegroundColor Yellow
        }
        
    } catch {
        Write-Host "[ERROR] Failed to fetch status: $_" -ForegroundColor Red
    }
    
    Write-Host ""
    Write-Host "======================================================" -ForegroundColor Cyan
    Write-Host "Refresh in $RefreshSeconds seconds..." -ForegroundColor Gray
    
    Start-Sleep -Seconds $RefreshSeconds
}
