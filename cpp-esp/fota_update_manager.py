"""
FOTA Update Manager with Rollback Support

This script provides a complete FOTA update management system including:
- Firmware upload to server
- Update monitoring
- Rollback capability
- Update verification
- Progress tracking
"""

import requests
import json
import hashlib
import base64
import time
import os
from datetime import datetime
from typing import Optional, Dict, Any

class FOTAUpdateManager:
    def __init__(self, server_url: str = "http://localhost:8080"):
        self.server_url = server_url
        self.firmware_path = ".pio/build/esp32dev/firmware.bin"
        
    def calculate_hash(self, data: bytes) -> str:
        """Calculate SHA256 hash of firmware data"""
        return hashlib.sha256(data).hexdigest()
    
    def read_firmware(self, custom_path: Optional[str] = None) -> Optional[bytes]:
        """Read firmware binary from file"""
        path = custom_path if custom_path else self.firmware_path
        
        try:
            with open(path, 'rb') as f:
                firmware_data = f.read()
            print(f"✅ Firmware loaded: {len(firmware_data)} bytes ({len(firmware_data)//1024}KB)")
            return firmware_data
        except FileNotFoundError:
            print(f"❌ Error: {path} not found! Build the project first with 'pio run'")
            return None
        except Exception as e:
            print(f"❌ Error reading firmware: {e}")
            return None
    
    def upload_firmware(self, version: str, firmware_data: bytes, 
                       chunk_size: int = 16384, description: str = "") -> bool:
        """
        Upload firmware to server
        
        Args:
            version: Firmware version (e.g., "1.0.6")
            firmware_data: Binary firmware data
            chunk_size: Chunk size in bytes (default 16KB)
            description: Optional description of this firmware version
        
        Returns:
            True if upload successful, False otherwise
        """
        fw_hash = self.calculate_hash(firmware_data)
        total_chunks = (len(firmware_data) + chunk_size - 1) // chunk_size
        
        payload = {
            "version": version,
            "size": len(firmware_data),
            "hash": fw_hash,
            "chunk_size": chunk_size,
            "description": description,
            "firmware_data": base64.b64encode(firmware_data).decode('ascii')
        }
        
        print(f"\n{'='*60}")
        print(f"📦 UPLOADING FIRMWARE")
        print(f"{'='*60}")
        print(f"Version:     {version}")
        print(f"Size:        {len(firmware_data)} bytes ({len(firmware_data)//1024}KB)")
        print(f"Hash:        {fw_hash[:32]}...")
        print(f"Chunk size:  {chunk_size} bytes ({chunk_size//1024}KB)")
        print(f"Total chunks: {total_chunks}")
        if description:
            print(f"Description: {description}")
        print(f"{'='*60}\n")
        
        try:
            print("⏳ Uploading to server...")
            response = requests.post(
                f"{self.server_url}/api/cloud/fota/upload",
                json=payload,
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                print(f"✅ Upload successful!")
                print(f"   Server response: {result}")
                return True
            else:
                print(f"❌ Upload failed: HTTP {response.status_code}")
                print(f"   Response: {response.text}")
                return False
                
        except requests.exceptions.Timeout:
            print(f"❌ Upload timeout! Server might be processing...")
            return False
        except Exception as e:
            print(f"❌ Upload error: {e}")
            return False
    
    def trigger_update(self, device_id: Optional[str] = None) -> bool:
        """
        Trigger FOTA update on device
        
        Args:
            device_id: Optional specific device ID to update
        
        Returns:
            True if trigger successful
        """
        print(f"\n{'='*60}")
        print(f"🚀 TRIGGERING FOTA UPDATE")
        print(f"{'='*60}")
        
        if device_id:
            print(f"Target device: {device_id}")
        else:
            print(f"Target: All devices checking for updates")
        
        print("\n💡 Please press the EN button on your ESP32 to check for updates")
        print(f"{'='*60}\n")
        return True
    
    def check_update_status(self, timeout: int = 300) -> Dict[str, Any]:
        """
        Monitor update status
        
        Args:
            timeout: Maximum time to wait in seconds (default 5 minutes)
        
        Returns:
            Dictionary with update status information
        """
        print(f"\n{'='*60}")
        print(f"📊 MONITORING UPDATE STATUS")
        print(f"{'='*60}")
        print(f"Timeout: {timeout} seconds ({timeout//60} minutes)")
        print(f"{'='*60}\n")
        
        start_time = time.time()
        last_check = 0
        
        while (time.time() - start_time) < timeout:
            current_time = time.time() - start_time
            
            # Print status every 10 seconds
            if current_time - last_check >= 10:
                elapsed_mins = int(current_time // 60)
                elapsed_secs = int(current_time % 60)
                print(f"⏱️  Elapsed: {elapsed_mins:02d}:{elapsed_secs:02d} - Monitoring...")
                last_check = current_time
            
            time.sleep(5)
        
        print(f"\n⚠️  Monitoring timeout reached")
        return {
            "status": "timeout",
            "message": "Manual verification required"
        }
    
    def rollback_firmware(self, reason: str = "Update verification failed") -> bool:
        """
        Initiate firmware rollback
        
        Args:
            reason: Reason for rollback
        
        Returns:
            True if rollback initiated successfully
        """
        print(f"\n{'='*60}")
        print(f"🔄 INITIATING FIRMWARE ROLLBACK")
        print(f"{'='*60}")
        print(f"Reason: {reason}")
        print(f"Time:   {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"{'='*60}\n")
        
        try:
            # Send rollback command to server
            response = requests.post(
                f"{self.server_url}/api/cloud/fota/rollback",
                json={"reason": reason},
                timeout=10
            )
            
            if response.status_code == 200:
                print("✅ Rollback command sent to server")
                print("\n📝 ROLLBACK PROCEDURE:")
                print("   1. Device will reboot")
                print("   2. Bootloader will detect rollback flag")
                print("   3. Previous firmware partition will be activated")
                print("   4. Device boots from previous working firmware")
                print(f"\n{'='*60}\n")
                return True
            else:
                print(f"❌ Rollback failed: HTTP {response.status_code}")
                return False
                
        except Exception as e:
            print(f"❌ Rollback error: {e}")
            print("\n⚠️  MANUAL ROLLBACK REQUIRED:")
            print("   1. Power cycle the ESP32")
            print("   2. ESP32 OTA bootloader should detect boot failure")
            print("   3. Automatic rollback to previous partition")
            print(f"\n{'='*60}\n")
            return False
    
    def verify_update(self, expected_version: str, timeout: int = 60) -> bool:
        """
        Verify that update was successful
        
        Args:
            expected_version: Expected firmware version after update
            timeout: Maximum time to wait for verification
        
        Returns:
            True if verification successful
        """
        print(f"\n{'='*60}")
        print(f"✔️  VERIFYING UPDATE")
        print(f"{'='*60}")
        print(f"Expected version: {expected_version}")
        print(f"Timeout: {timeout} seconds")
        print(f"{'='*60}\n")
        
        print("⏳ Waiting for device to boot and connect...")
        time.sleep(10)
        
        start_time = time.time()
        
        while (time.time() - start_time) < timeout:
            try:
                # Try to get device status
                response = requests.get(
                    f"{self.server_url}/api/inverter/status",
                    timeout=5
                )
                
                if response.status_code == 200:
                    status = response.json()
                    current_version = status.get('firmware_version', 'unknown')
                    
                    print(f"📡 Device responded!")
                    print(f"   Current version: {current_version}")
                    
                    if current_version == expected_version:
                        print(f"✅ Update verified successfully!")
                        print(f"   Device is running expected version: {expected_version}")
                        return True
                    else:
                        print(f"⚠️  Version mismatch!")
                        print(f"   Expected: {expected_version}")
                        print(f"   Actual:   {current_version}")
                        return False
                        
            except requests.exceptions.RequestException:
                pass  # Device not responding yet
            
            time.sleep(5)
        
        print(f"❌ Verification timeout")
        print(f"   Device did not respond within {timeout} seconds")
        return False
    
    def perform_complete_update(self, version: str, 
                               firmware_path: Optional[str] = None,
                               description: str = "",
                               verify: bool = True,
                               auto_rollback: bool = True) -> bool:
        """
        Perform complete FOTA update with verification and rollback
        
        Args:
            version: Firmware version
            firmware_path: Optional custom firmware path
            description: Update description
            verify: Whether to verify update
            auto_rollback: Whether to auto-rollback on failure
        
        Returns:
            True if update successful
        """
        print(f"\n{'#'*60}")
        print(f"# FOTA UPDATE PROCESS - VERSION {version}")
        print(f"{'#'*60}\n")
        
        # Step 1: Read firmware
        print("STEP 1: Reading firmware...")
        firmware_data = self.read_firmware(firmware_path)
        if not firmware_data:
            return False
        
        # Step 2: Upload firmware
        print("\nSTEP 2: Uploading firmware...")
        if not self.upload_firmware(version, firmware_data, 
                                    chunk_size=16384, description=description):
            return False
        
        # Step 3: Trigger update
        print("\nSTEP 3: Triggering update...")
        self.trigger_update()
        
        # Step 4: Wait for download
        print("\nSTEP 4: Waiting for download...")
        print("⏳ Please wait while device downloads firmware...")
        print("   (Monitor serial output for progress)")
        
        # Calculate expected download time
        total_chunks = (len(firmware_data) + 16383) // 16384
        estimated_time = total_chunks * 2  # 2 seconds per chunk
        print(f"   Estimated time: ~{estimated_time} seconds ({estimated_time//60} min {estimated_time%60} sec)")
        
        # Add extra time for verification and reboot
        wait_time = estimated_time + 30
        print(f"\n   Waiting {wait_time} seconds...")
        time.sleep(wait_time)
        
        # Step 5: Verify update
        if verify:
            print("\nSTEP 5: Verifying update...")
            if self.verify_update(version):
                print(f"\n{'#'*60}")
                print(f"# ✅ UPDATE SUCCESSFUL!")
                print(f"{'#'*60}\n")
                return True
            else:
                print(f"\n{'#'*60}")
                print(f"# ⚠️  UPDATE VERIFICATION FAILED!")
                print(f"{'#'*60}\n")
                
                if auto_rollback:
                    print("Initiating automatic rollback...")
                    self.rollback_firmware("Update verification failed")
                
                return False
        else:
            print("\nSTEP 5: Verification skipped")
            print(f"\n{'#'*60}")
            print(f"# ⚠️  UPDATE COMPLETED (NOT VERIFIED)")
            print(f"{'#'*60}\n")
            return True


def main():
    """Main function with interactive menu"""
    manager = FOTAUpdateManager()
    
    while True:
        print(f"\n{'='*60}")
        print("FOTA UPDATE MANAGER")
        print(f"{'='*60}")
        print("1. Upload firmware (quick)")
        print("2. Perform complete update with verification")
        print("3. Trigger update (manual)")
        print("4. Monitor update status")
        print("5. Rollback firmware")
        print("6. Verify current firmware version")
        print("0. Exit")
        print(f"{'='*60}")
        
        choice = input("\nSelect option (0-6): ").strip()
        
        if choice == "1":
            # Quick upload
            version = input("Enter firmware version (e.g., 1.0.6): ").strip()
            description = input("Enter description (optional): ").strip()
            
            firmware_data = manager.read_firmware()
            if firmware_data:
                manager.upload_firmware(version, firmware_data, 
                                       chunk_size=16384, description=description)
        
        elif choice == "2":
            # Complete update
            version = input("Enter firmware version (e.g., 1.0.6): ").strip()
            description = input("Enter description (optional): ").strip()
            
            print("\nOptions:")
            print("  Verify update: yes")
            print("  Auto-rollback on failure: yes")
            confirm = input("\nProceed with update? (yes/no): ").strip().lower()
            
            if confirm == "yes":
                manager.perform_complete_update(
                    version=version,
                    description=description,
                    verify=True,
                    auto_rollback=True
                )
        
        elif choice == "3":
            # Trigger update
            device_id = input("Enter device ID (or press Enter for all): ").strip()
            manager.trigger_update(device_id if device_id else None)
        
        elif choice == "4":
            # Monitor status
            timeout = input("Enter timeout in seconds (default 300): ").strip()
            timeout = int(timeout) if timeout else 300
            manager.check_update_status(timeout)
        
        elif choice == "5":
            # Rollback
            reason = input("Enter rollback reason: ").strip()
            manager.rollback_firmware(reason if reason else "Manual rollback")
        
        elif choice == "6":
            # Verify version
            expected = input("Enter expected version: ").strip()
            manager.verify_update(expected)
        
        elif choice == "0":
            print("\n👋 Exiting...")
            break
        
        else:
            print("❌ Invalid option!")


if __name__ == "__main__":
    # Quick test mode - uncomment to test specific function
    # manager = FOTAUpdateManager()
    # manager.perform_complete_update(version="1.0.6", description="Test update with rollback")
    
    # Interactive mode
    main()
