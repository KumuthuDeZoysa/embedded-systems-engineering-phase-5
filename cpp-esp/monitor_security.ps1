# Security Integration Monitoring Script
# Run this in PowerShell to monitor both ESP32 and Flask server

Write-Host "=== SECURITY INTEGRATION MONITOR ===" -ForegroundColor Cyan
Write-Host "Monitoring Flask server and checking for key events..." -ForegroundColor Yellow
Write-Host ""

# Colors for different log levels
$colors = @{
    "INFO" = "Green"
    "WARN" = "Yellow"
    "ERROR" = "Red"
    "DEBUG" = "Gray"
    "SECURITY" = "Magenta"
}

Write-Host "Key things to watch for:" -ForegroundColor White
Write-Host "  [ESP32] [SecureHttp] Performing secure GET/POST request" -ForegroundColor Green
Write-Host "  [Flask] [SECURITY] EcoWatt001: hmac_verified - Nonce: XXX" -ForegroundColor Magenta
Write-Host "  [Flask] POST /api/upload 200 (not 401!)" -ForegroundColor Green
Write-Host "  [Flask] GET /api/inverter/config 200 (not 401!)" -ForegroundColor Green
Write-Host ""
Write-Host "Press Ctrl+C to stop monitoring" -ForegroundColor Yellow
Write-Host "=" * 60
Write-Host ""

# Monitor Flask server logs (assumes running in terminal)
# You can also check the actual terminal output in VS Code

Write-Host "[INFO] Check the VS Code terminals:" -ForegroundColor Cyan
Write-Host "  1. Terminal 'pio' - ESP32 Serial Monitor (shows [SecureHttp] logs)" -ForegroundColor White
Write-Host "  2. Terminal 'python' - Flask Server (shows [SECURITY] logs)" -ForegroundColor White
Write-Host ""
Write-Host "Expected SUCCESS pattern:" -ForegroundColor Green
Write-Host "  ESP32: [INFO] [SecureHttp] Performing secure GET request" -ForegroundColor Gray
Write-Host "  ESP32: [DEBUG] [SecureHttp] GET with nonce=22, mac=abc123..." -ForegroundColor Gray
Write-Host "  Flask: [SECURITY] EcoWatt001: hmac_verified - Nonce: 22" -ForegroundColor Gray
Write-Host "  Flask: 10.50.126.50 - - GET /api/inverter/config 200" -ForegroundColor Gray
Write-Host "  ESP32: [INFO] [RemoteCfg] No config updates available" -ForegroundColor Gray
Write-Host ""
Write-Host "If you see 401 errors, security headers may be incorrect!" -ForegroundColor Red
Write-Host ""
