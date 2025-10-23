#!/usr/bin/env python3
"""
Quick script to send configuration update to Flask server
"""
import requests
import json

# Configuration to send
config_payload = {
    "device_id": "EcoWatt001",
    "sampling_interval": 10,  # Change from 5 to 10 seconds
    "registers": [0, 1, 8]     # Only registers 0, 1, and 8 (add register 8)
}

# Flask server endpoint
url = "http://10.50.126.183:8080/api/cloud/config/send"

print("=" * 60)
print("Sending Configuration Update to Flask Server")
print("=" * 60)
print(f"Endpoint: {url}")
print(f"Payload:\n{json.dumps(config_payload, indent=2)}")
print("=" * 60)

try:
    response = requests.post(url, json=config_payload, timeout=5)
    
    print(f"\nStatus Code: {response.status_code}")
    print(f"Response:\n{json.dumps(response.json(), indent=2)}")
    
    if response.status_code == 200:
        print("\n✅ Configuration sent successfully!")
        print("\n📡 ESP32 will pick it up in the next check cycle (60 seconds)")
        print("    Watch the serial monitor for cyan [CONFIG] messages")
    else:
        print(f"\n❌ Failed to send configuration: {response.status_code}")
        
except requests.exceptions.RequestException as e:
    print(f"\n❌ Error: {e}")
    print("\nMake sure Flask server is running at http://10.50.126.183:8080")
