import requests
import json
import hashlib
import base64

def upload_firmware():
    # Use the COMPLETE REAL compiled firmware binary (first 10KB for testing)
    firmware_path = ".pio/build/esp32dev/firmware.bin"
    
    try:
        with open(firmware_path, 'rb') as f:
            full_firmware = f.read()
            # Take only first 10KB (10 chunks of 1KB each) for quick testing
            firmware_data = full_firmware[:10240]  # 10KB = 10 chunks × 1KB
    except FileNotFoundError:
        print(f"❌ Error: {firmware_path} not found! Build the project first with 'pio run'")
        return
    
    # Calculate hash
    fw_hash = hashlib.sha256(firmware_data).hexdigest()
    
    # Prepare payload
    payload = {
        "version": "1.0.5",
        "size": len(firmware_data),
        "hash": fw_hash,
        "chunk_size": 1024,  # 1KB chunks
        "firmware_data": base64.b64encode(firmware_data).decode('ascii')
    }
    
    print(f"📦 Uploading REAL firmware v1.0.5 (10KB TEST)")
    print(f"   Size: {len(firmware_data)} bytes ({len(firmware_data)//1024}KB)")
    print(f"   Hash: {fw_hash[:16]}...")
    print(f"   Chunks: {(len(firmware_data) + 1023) // 1024} chunks of 1KB each")
    
    # Upload to cloud
    try:
        response = requests.post("http://localhost:8080/api/cloud/fota/upload", 
                               json=payload, timeout=10)
        print(f"Response: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"✅ Success! Total chunks: {result.get('total_chunks')}")
        else:
            print(f"❌ Error: {response.text}")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    upload_firmware()