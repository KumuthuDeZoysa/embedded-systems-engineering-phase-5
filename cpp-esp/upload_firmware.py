#!/usr/bin/env python3
"""
Simple FOTA Firmware Upload Script
"""

import requests
import json
import time
import hashlib
import base64

def create_test_firmware(version="1.0.4", size=40960):
    """Create a test firmware binary for demo purposes (40KB = 10 chunks)."""
    print(f"Creating test firmware: version {version}, size {size} bytes ({size//1024} KB)")
    
    # Create realistic firmware content with proper ESP32 headers
    firmware_data = bytearray(size)
    
    # ESP32 app header (simplified)
    firmware_data[0:4] = b'\xe9\x00\x00\x20'  # ESP32 app magic
    firmware_data[4:8] = version.encode('utf-8')[:4].ljust(4, b'\x00')
    
    # Fill with realistic firmware-like data
    for i in range(32, size, 4):
        # Create some pattern that looks like compiled code
        value = (i * 0x12345678) & 0xFFFFFFFF
        if i + 4 <= size:
            firmware_data[i:i+4] = value.to_bytes(4, 'little')
    
    # Add version string at the end
    version_bytes = f"EcoWatt-{version}-demo".encode('utf-8')
    if len(version_bytes) <= 32:
        firmware_data[-32:-32+len(version_bytes)] = version_bytes
    
    return bytes(firmware_data)

def upload_firmware(cloud_url="http://localhost:8080"):
    """Upload firmware to cloud for FOTA distribution."""
    firmware_data = create_test_firmware("1.0.4", 35840)
    print(f"Uploading firmware version 1.0.4 ({len(firmware_data)} bytes)")
    
    # Calculate SHA-256 hash
    fw_hash = hashlib.sha256(firmware_data).hexdigest()
    print(f"Firmware hash: {fw_hash}")
    
    # Encode as base64
    firmware_b64 = base64.b64encode(firmware_data).decode('ascii')
    
    payload = {
        "version": "1.0.4",
        "size": len(firmware_data),
        "hash": fw_hash,
        "chunk_size": 1024,
        "firmware_data": firmware_b64
    }
    
    try:
        response = requests.post(f"{cloud_url}/api/cloud/fota/upload", 
                               json=payload, timeout=30)
        response.raise_for_status()
        result = response.json()
        
        if result.get('status') == 'success':
            print(f"✅ Firmware uploaded successfully!")
            print(f"Total chunks created: {result.get('total_chunks', 0)}")
            return True
        else:
            print(f"❌ Upload failed: {result}")
            return False
            
    except Exception as e:
        print(f"❌ Upload error: {e}")
        return False

if __name__ == '__main__':
    print("FOTA Firmware Upload")
    upload_firmware()