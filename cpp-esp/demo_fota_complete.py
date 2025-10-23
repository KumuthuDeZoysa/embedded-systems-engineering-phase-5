#!/usr/bin/env python3
"""
Complete FOTA Demonstration Script
==================================
This script:
1. Uploads firmware to Flask server
2. Waits for ESP32 to detect and download it
3. Shows the complete chunked download process
"""

import requests
import json
import hashlib
import base64
import time
from pathlib import Path

# Configuration
SERVER_URL = "http://localhost:8080"
FIRMWARE_VERSION = "1.0.4"
FIRMWARE_PATH = ".pio/build/esp32dev/firmware.bin"

def upload_firmware():
    """Upload firmware binary to Flask server for FOTA distribution."""
    print("=" * 60)
    print("🚀 FOTA DEMONSTRATION - Firmware Upload")
    print("=" * 60)
    
    # Check if firmware file exists
    firmware_file = Path(FIRMWARE_PATH)
    if not firmware_file.exists():
        print(f"❌ Firmware file not found: {FIRMWARE_PATH}")
        print("   Please build the project first: pio run")
        return False
    
    # Read firmware binary
    print(f"\n📦 Reading firmware from: {FIRMWARE_PATH}")
    with open(firmware_file, 'rb') as f:
        firmware_data = f.read()
    
    file_size = len(firmware_data)
    print(f"   Firmware size: {file_size:,} bytes")
    
    # Calculate SHA-256 hash
    fw_hash = hashlib.sha256(firmware_data).hexdigest()
    print(f"   SHA-256 hash: {fw_hash[:32]}...")
    
    # Encode as base64
    firmware_b64 = base64.b64encode(firmware_data).decode('ascii')
    
    # Prepare upload payload
    payload = {
        "version": FIRMWARE_VERSION,
        "size": file_size,
        "hash": fw_hash,
        "firmware_data": firmware_b64,
        "hw_variant": "esp32-dev",
        "chunk_size": 1024  # 1KB chunks for ESP32 memory efficiency
    }
    
    print(f"\n📤 Uploading firmware to server: {SERVER_URL}")
    print(f"   Version: {FIRMWARE_VERSION}")
    print(f"   Chunk size: 1024 bytes")
    
    try:
        response = requests.post(
            f"{SERVER_URL}/api/cloud/fota/upload",
            json=payload,
            timeout=30
        )
        
        if response.status_code == 200:
            result = response.json()
            print("\n✅ Firmware uploaded successfully!")
            print(f"   Version: {result.get('version')}")
            print(f"   Size: {result.get('size'):,} bytes")
            print(f"   Total chunks: {result.get('chunks')}")
            print(f"   Chunk size: {result.get('chunk_size')} bytes")
            return True
        else:
            print(f"\n❌ Upload failed: {response.status_code}")
            print(f"   Error: {response.text}")
            return False
            
    except requests.exceptions.ConnectionError:
        print(f"\n❌ Cannot connect to server at {SERVER_URL}")
        print("   Please start the Flask server first:")
        print("   > python app.py")
        return False
    except Exception as e:
        print(f"\n❌ Upload error: {e}")
        return False

def monitor_fota_progress():
    """Monitor FOTA download progress."""
    print("\n" + "=" * 60)
    print("📡 MONITORING FOTA DOWNLOAD")
    print("=" * 60)
    
    print("\n🔍 Watch your ESP32 serial monitor for these messages:")
    print("\n   Expected sequence:")
    print("   1. [FOTA] Checking for firmware updates")
    print("   2. [FOTA] New firmware available: 1.0.4")
    print("   3. [FOTA] Size: XXXXX bytes, Chunks: XX")
    print("   4. [FOTA] Starting firmware download")
    print("   5. [FOTA] Downloading chunk 0/N (1024 bytes)")
    print("   6. [FOTA] Downloading chunk 1/N (1024 bytes)")
    print("   7. ... (progress continues)")
    print("   8. [FOTA] Download complete, verifying SHA-256...")
    print("   9. [FOTA] Verification successful")
    print("  10. [FOTA] Firmware update applied, rebooting...")
    
    print("\n⏱️  Timeline:")
    print("   • ESP32 checks for updates every ~23 seconds")
    print("   • Download starts immediately when update detected")
    print("   • Chunked download (1KB chunks) takes ~30-60 seconds")
    print("   • Device reboots automatically after successful update")
    
    print("\n💡 Troubleshooting:")
    print("   • If no update detected: Wait up to 23 seconds for next check")
    print("   • If download fails: Check WiFi connection")
    print("   • If verification fails: Firmware may be corrupted")
    
    print("\n" + "=" * 60)

def main():
    print("\n" + "="* 60)
    print("  ECOWATT FOTA DEMONSTRATION")
    print("  Firmware Over-The-Air Update with Chunked Download")
    print("=" * 60)
    
    # Step 1: Upload firmware
    if not upload_firmware():
        print("\n❌ FOTA demonstration failed - cannot upload firmware")
        return
    
    # Step 2: Monitoring instructions
    monitor_fota_progress()
    
    print("\n✅ FOTA demonstration ready!")
    print("\n📝 Next steps:")
    print("   1. Open serial monitor: pio device monitor --port COM5 --baud 115200")
    print("   2. Wait for ESP32 to detect update (max 23 seconds)")
    print("   3. Watch chunked download progress in real-time")
    print("   4. Observe firmware verification and reboot")
    
    print("\n🎬 This is what you should demonstrate in your video!")
    print("=" * 60)

if __name__ == "__main__":
    main()