#!/usr/bin/env python3
"""
Upload firmware with CORRUPTED HASH to demonstrate FOTA rollback behavior.
The device will download all chunks but FAIL verification, demonstrating safe rollback.
"""

import requests
import hashlib
import base64
import os

# Configuration
CLOUD_SERVER = "http://localhost:8080"
FIRMWARE_PATH = ".pio/build/esp32dev/firmware.bin"

def upload_corrupted_firmware(version: str):
    """Upload real firmware but with intentionally wrong hash to trigger verification failure."""
    
    if not os.path.exists(FIRMWARE_PATH):
        print(f"❌ Firmware not found at {FIRMWARE_PATH}")
        print("   Run 'pio run' first to compile the firmware")
        return False
    
    # Read the actual firmware
    with open(FIRMWARE_PATH, "rb") as f:
        firmware_data = f.read()
    
    firmware_size = len(firmware_data)
    
    # Calculate the REAL hash
    real_hash = hashlib.sha256(firmware_data).hexdigest()
    
    # Create a FAKE/CORRUPTED hash (flip some characters)
    corrupted_hash = "deadbeef" + real_hash[8:56] + "cafebabe"
    
    print(f"=" * 60)
    print(f"  FOTA FAILURE + ROLLBACK DEMO")
    print(f"=" * 60)
    print(f"  Version:        {version}")
    print(f"  Firmware Size:  {firmware_size:,} bytes")
    print(f"  Real SHA256:    {real_hash[:32]}...")
    print(f"  FAKE SHA256:    {corrupted_hash[:32]}...")
    print(f"=" * 60)
    print()
    print("⚠️  This will upload firmware with WRONG HASH!")
    print("⚠️  Device will download but FAIL verification!")
    print()
    
    # Encode firmware as base64 (same as upload_real_firmware.py)
    firmware_b64 = base64.b64encode(firmware_data).decode('ascii')
    
    # Use 8KB chunks like the working version
    chunk_size = 8192
    num_chunks = (firmware_size + chunk_size - 1) // chunk_size
    
    print(f"📦 Uploading to server ({num_chunks} chunks)...")
    
    # Use the same endpoint format as upload_real_firmware.py
    payload = {
        "version": version,
        "size": firmware_size,
        "hash": corrupted_hash,  # ← INTENTIONALLY WRONG!
        "chunk_size": chunk_size,
        "firmware_data": firmware_b64,
        "skip_validation": True  # Skip server-side validation for rollback test
    }
    
    try:
        response = requests.post(
            f"{CLOUD_SERVER}/api/cloud/fota/upload",
            json=payload,
            timeout=60
        )
        
        if response.status_code != 200:
            print(f"❌ Failed to upload: {response.text}")
            return False
        
        result = response.json()
        
        if result.get('status') == 'success':
            print()
            print(f"✅ Corrupted firmware v{version} uploaded successfully!")
            print(f"   Total chunks: {result.get('total_chunks', num_chunks)}")
            print()
            print(f"=" * 60)
            print(f"  WHAT TO EXPECT IN SERIAL MONITOR:")
            print(f"=" * 60)
            print(f"  1. [FOTA] New version available: {version}")
            print(f"  2. Download progress ({num_chunks} chunks)")
            print(f"  3. [FOTA] ✗ Firmware verification FAILED!")
            print(f"  4. [FOTA] Hash mismatch - expected vs calculated")
            print(f"  5. Device stays on current version (NO reboot)")
            print(f"=" * 60)
            return True
        else:
            print(f"❌ Upload failed: {result}")
            return False
        
    except requests.exceptions.ConnectionError:
        print(f"❌ Cannot connect to {CLOUD_SERVER}")
        print("   Make sure Flask server is running: python app.py")
        return False

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python upload_firmware_corrupted.py <version>")
        print("Example: python upload_firmware_corrupted.py 1.2.0")
        sys.exit(1)
    
    version = sys.argv[1]
    success = upload_corrupted_firmware(version)
    sys.exit(0 if success else 1)
