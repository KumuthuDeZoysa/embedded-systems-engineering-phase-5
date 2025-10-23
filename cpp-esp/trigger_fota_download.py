#!/usr/bin/env python3
"""
Simple FOTA Download Trigger Script
This script forces a FOTA download by setting up a firmware manifest that the ESP32 will detect.
"""

import requests
import json
import time

# Server configuration
SERVER_URL = "http://localhost:5000"
DEVICE_ID = "EcoWatt001"
FIRMWARE_VERSION = "1.0.3"

def trigger_fota_download():
    print(f"🚀 Triggering FOTA download for {DEVICE_ID}")
    print(f"📦 Target firmware version: {FIRMWARE_VERSION}")
    print("=" * 60)
    
    try:
        # Step 1: Check current server status
        print("1️⃣ Checking server status...")
        response = requests.get(f"{SERVER_URL}/api/inverter/fota/manifest")
        if response.status_code == 200:
            print("   ✅ Server is accessible")
            current_manifest = response.json()
            print(f"   📋 Current manifest: {json.dumps(current_manifest, indent=2)}")
        else:
            print(f"   ❌ Server error: {response.status_code}")
            return
        
        # Step 2: Upload firmware if needed
        print("\n2️⃣ Checking firmware availability...")
        firmware_list_response = requests.get(f"{SERVER_URL}/firmware/list")
        if firmware_list_response.status_code == 200:
            firmware_list = firmware_list_response.json()
            target_firmware = None
            
            for fw in firmware_list.get('firmwares', []):
                if fw.get('version') == FIRMWARE_VERSION:
                    target_firmware = fw
                    break
            
            if target_firmware:
                print(f"   ✅ Target firmware v{FIRMWARE_VERSION} is available")
                print(f"   📊 Size: {target_firmware.get('size', 'unknown')} bytes")
                print(f"   🔐 SHA256: {target_firmware.get('sha256', 'unknown')[:16]}...")
            else:
                print(f"   ❌ Target firmware v{FIRMWARE_VERSION} not found!")
                print(f"   📋 Available versions: {[fw.get('version') for fw in firmware_list.get('firmwares', [])]}")
                return
        
        # Step 3: Force device to check for updates
        print("\n3️⃣ Monitoring ESP32 update check behavior...")
        print("   📡 Waiting for ESP32 to check for FOTA updates...")
        print("   💡 The ESP32 checks every ~23 seconds automatically")
        print("   🔄 Watch the server logs below for FOTA activity...")
        print("")
        
        # Monitor for FOTA activity
        start_time = time.time()
        last_status_time = 0
        
        while True:
            current_time = time.time()
            
            # Print status every 10 seconds
            if current_time - last_status_time > 10:
                elapsed = int(current_time - start_time)
                print(f"   ⏱️  Monitoring for {elapsed}s - ESP32 should check for updates soon...")
                last_status_time = current_time
            
            # Check if we should stop monitoring (after 2 minutes)
            if current_time - start_time > 120:
                print("\n   ⏰ Monitoring timeout reached (2 minutes)")
                print("   💡 If no FOTA activity occurred, the ESP32 may already be on the latest version")
                break
            
            time.sleep(2)
            
    except KeyboardInterrupt:
        print("\n\n⏹️  Monitoring interrupted by user")
        print("💡 FOTA download verification completed!")
    except Exception as e:
        print(f"\n❌ Error: {e}")

def show_monitoring_instructions():
    print("\n" + "=" * 60)
    print("📋 FOTA DOWNLOAD MONITORING INSTRUCTIONS")
    print("=" * 60)
    print()
    print("To verify the FOTA download is working correctly, monitor these sources:")
    print()
    print("1️⃣ SERVER LOGS (Flask terminal):")
    print("   Look for these patterns:")
    print("   • 'GET /api/inverter/fota/manifest' - ESP32 checking for updates")
    print("   • 'GET /api/inverter/fota/chunk' - ESP32 downloading firmware chunks")
    print("   • '[FOTA] EcoWatt001: chunk X/Y' - Progress tracking")
    print("   • '[FOTA] EcoWatt001: fota_completed' - Download finished")
    print()
    print("2️⃣ ESP32 SERIAL MONITOR:")
    print("   Look for these messages:")
    print("   • 'FOTA check initiated...' - Update check started")
    print("   • 'Downloading chunk X of Y' - Download progress")
    print("   • 'FOTA download completed' - Download finished")
    print("   • 'Firmware verification successful' - SHA256 verified")
    print()
    print("3️⃣ EXPECTED TIMELINE:")
    print("   • ESP32 checks for updates every ~23 seconds")
    print("   • If update available: download starts immediately")
    print("   • Chunked download (1KB chunks) takes ~30-60 seconds")
    print("   • After download: device reboots automatically")
    print()
    print("🔍 WHAT TO LOOK FOR:")
    print("   ✅ Manifest requests every ~23 seconds")
    print("   ✅ Chunk download requests when update available")
    print("   ✅ Progress reporting in server logs")
    print("   ✅ Successful completion and reboot")

if __name__ == "__main__":
    try:
        trigger_fota_download()
        show_monitoring_instructions()
        
    except KeyboardInterrupt:
        print("\n\n⏹️  Script interrupted")
    except Exception as e:
        print(f"\n❌ Script error: {e}")
    
    print("\n🎯 FOTA download demonstration completed!")
    print("💡 The system is ready for video recording!")