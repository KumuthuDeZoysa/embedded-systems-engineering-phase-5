#!/usr/bin/env python3
"""
Upload REAL Firmware for FOTA Demo
Uses the actual compiled firmware.bin from PlatformIO build
"""

import requests
import json
import hashlib
import base64
import os

def upload_real_firmware(cloud_url="http://localhost:8080", version="1.0.5"):
    """Upload the real compiled firmware for FOTA."""
    
    firmware_path = ".pio/build/esp32dev/firmware.bin"
    
    if not os.path.exists(firmware_path):
        print(f"❌ Firmware not found: {firmware_path}")
        print("   Run 'pio run' to build the firmware first")
        return False
    
    # Read real firmware
    with open(firmware_path, "rb") as f:
        firmware_data = f.read()
    
    size = len(firmware_data)
    print(f"📦 Real Firmware Upload")
    print(f"=" * 60)
    print(f"   File: {firmware_path}")
    print(f"   Size: {size:,} bytes ({size/1024:.1f} KB)")
    print(f"   Version: {version}")
    
    # Calculate SHA-256 hash
    fw_hash = hashlib.sha256(firmware_data).hexdigest()
    print(f"   SHA256: {fw_hash[:32]}...")
    
    # Encode as base64
    firmware_b64 = base64.b64encode(firmware_data).decode('ascii')
    
    # Use 8KB chunks - fits in ESP32 memory, faster than 4KB
    # 8KB binary = ~11KB base64 in JSON = should fit in ESP32 RAM
    # 1MB / 8KB = ~132 chunks @ 0.5s each = ~66 seconds
    chunk_size = 8192  # 8KB per chunk
    num_chunks = (size + chunk_size - 1) // chunk_size
    print(f"   Chunks: {num_chunks} (each {chunk_size/1024:.1f} KB)")
    print(f"=" * 60)
    
    payload = {
        "version": version,
        "size": size,
        "hash": fw_hash,
        "chunk_size": chunk_size,
        "firmware_data": firmware_b64
    }
    
    print(f"\n⬆️  Uploading to {cloud_url}...")
    
    try:
        response = requests.post(f"{cloud_url}/api/cloud/fota/upload", 
                               json=payload, timeout=60)
        response.raise_for_status()
        result = response.json()
        
        if result.get('status') == 'success':
            print(f"\n✅ Firmware uploaded successfully!")
            print(f"   Total chunks: {result.get('total_chunks', 0)}")
            print(f"\n📡 Device will download on next boot or FOTA check")
            print(f"   Press RST button on ESP32 to trigger FOTA")
            return True
        else:
            print(f"❌ Upload failed: {result}")
            return False
            
    except Exception as e:
        print(f"❌ Upload error: {e}")
        return False

if __name__ == '__main__':
    import sys
    version = sys.argv[1] if len(sys.argv) > 1 else "1.0.5"
    upload_real_firmware(version=version)
