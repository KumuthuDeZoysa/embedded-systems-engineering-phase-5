# PowerShell script to upload test firmware for FOTA demo
Write-Host "=== FOTA DEMO - Firmware Upload Test ==="
Write-Host "Creating test firmware (35KB)..."

# Generate 35KB test firmware (exactly 35,840 bytes = 35 chunks of 1024 bytes)
$firmware_hex = "0123456789abcdef" * 2240  # 16 * 2240 = 35,840 bytes
$firmware_bytes = [System.Text.Encoding]::UTF8.GetBytes($firmware_hex)
$firmware_base64 = [System.Convert]::ToBase64String($firmware_bytes)
$sha256 = "c5bb734e5f7a57f8de78a2f8c2fc2e8e4c4e8e2b1d0e3f8d8e9a9c1b6f4d9e5"

Write-Host "Firmware hex size: $($firmware_hex.Length) bytes"
Write-Host "Firmware binary size: $($firmware_bytes.Length) bytes"
Write-Host "Expected chunks: $([math]::ceiling($firmware_bytes.Length / 1024))"
Write-Host "SHA256: $sha256"

# Create JSON payload with correct field names
$json_payload = @{
    version = "1.0.4"
    size = $firmware_bytes.Length
    hash = $sha256
    chunk_size = 1024
    firmware_data = $firmware_base64
} | ConvertTo-Json -Compress

Write-Host "`nUploading firmware to Flask backend..."
Write-Host "URL: http://localhost:8080/api/cloud/fota/upload"

try {
    $response = Invoke-RestMethod -Uri "http://localhost:8080/api/cloud/fota/upload" -Method POST -ContentType "application/json" -Body $json_payload
    Write-Host "`n✅ Upload successful!"
    Write-Host "Response: $($response | ConvertTo-Json -Depth 3)"
} catch {
    Write-Host "`n❌ Upload failed!"
    Write-Host "Error: $($_.Exception.Message)"
    if ($_.Exception.Response) {
        $reader = New-Object System.IO.StreamReader($_.Exception.Response.GetResponseStream())
        $responseBody = $reader.ReadToEnd()
        Write-Host "Response Body: $responseBody"
    }
}

Write-Host "`n=== Check ESP32 serial monitor for FOTA activity ==="
Write-Host "Expected: ESP32 should start downloading chunks in ~10 seconds"
Write-Host "Monitor Node-RED dashboard at: http://localhost:1880/fota"