# Restart Flask with clean nonce store
Write-Host "Killing Flask processes..." -ForegroundColor Yellow
taskkill /F /IM python.exe 2>&1 | Out-Null

Start-Sleep -Seconds 2

Write-Host "Starting Flask with 75-second nonce expiry..." -ForegroundColor Green
cd "E:\ES Phase 4\embedded-systems-engineering-phase-4"
python cpp-esp\app.py
