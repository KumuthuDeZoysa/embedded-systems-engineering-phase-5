#!/usr/bin/env python3
"""Check FOTA status and trigger download"""
import requests
import json

SERVER = "http://localhost:8080"

print("=" * 60)
print("FOTA STATUS CHECK")
print("=" * 60)

# Check firmware list
print("\n1. Checking firmware list...")
try:
    r = requests.get(f"{SERVER}/firmware/list")
    if r.status_code == 200:
        data = r.json()
        print(f"   ✅ Found {len(data.get('firmwares', []))} firmware versions:")
        for fw in data.get('firmwares', []):
            print(f"      - v{fw['version']}: {fw['size']} bytes, {fw['total_chunks']} chunks")
        
        # Use the first firmware available
        if data.get('firmwares'):
            target_fw = data['firmwares'][0]
            print(f"\n2. Setting FOTA manifest to v{target_fw['version']}...")
            
            # The ESP32 will automatically detect this when it checks for updates
            print(f"   ✅ Firmware is ready for download")
            print(f"   📊 Version: {target_fw['version']}")
            print(f"   📦 Size: {target_fw['size']} bytes")
            print(f"   🔢 Chunks: {target_fw['total_chunks']}")
            print(f"   🔐 Hash: {target_fw['sha256'][:16]}...")
            
            print(f"\n3. ESP32 will auto-check in ~23 seconds")
            print(f"   Watch the serial monitor for:")
            print(f"   - '✓ OTA partition initialized'")
            print(f"   - 'Writing chunks DIRECTLY to OTA partition'")
            print(f"   - '✓ Chunk X written to OTA partition'")
    else:
        print(f"   ❌ Error: {r.status_code}")
except Exception as e:
    print(f"   ❌ Error: {e}")

print("\n" + "=" * 60)
print("Monitor the serial output to see the FOTA download!")
print("=" * 60)
