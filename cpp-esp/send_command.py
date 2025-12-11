#!/usr/bin/env python3
"""
Feature 4: Command Execution Demo Script
Sends a write command to the ESP32 device via the Flask cloud server.
The device will write the value to the specified register on the Inverter SIM.
"""

import requests
import json
import argparse
import sys

# Flask server endpoint
SERVER_URL = "http://10.81.6.183:8080"

# Available registers that can be written (from Inverter SIM API)
WRITABLE_REGISTERS = {
    8: "Battery SOC (%)",
    9: "Output Power (W)", 
    10: "Input Power (W)",
    # Add more as needed
}

def send_command(register: int, value: float, device_id: str = "EcoWatt001", encrypted: bool = False):
    """Send a write_register command to the device."""
    
    endpoint = f"{SERVER_URL}/api/cloud/command/send"
    
    payload = {
        "device_id": device_id,
        "action": "write_register",
        "target_register": str(register),
        "value": value,
        "encrypted": encrypted
    }
    
    print("=" * 60)
    print("Feature 4: Command Execution Demo")
    print("=" * 60)
    print(f"Endpoint: {endpoint}")
    print(f"Command: Write register {register} = {value}")
    if register in WRITABLE_REGISTERS:
        print(f"Register: {WRITABLE_REGISTERS[register]}")
    print(f"Device: {device_id}")
    print(f"Encrypted: {encrypted}")
    print("=" * 60)
    
    try:
        response = requests.post(endpoint, json=payload, timeout=10)
        response.raise_for_status()
        
        result = response.json()
        print("\n✅ Command sent successfully!")
        print(f"   Status: {result.get('status')}")
        print(f"   Message: {result.get('message')}")
        print(f"   Nonce: {result.get('nonce')}")
        print("\n📡 The device will execute this command on its next poll cycle.")
        print("   Watch the serial monitor for:")
        print("   - [CommandExec] Executing command...")
        print("   - [CommandExec] Write register X = Y")
        print("   - [CommandExec] Command result: success/failed")
        
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"\n❌ Error: {e}")
        print(f"Make sure Flask server is running at {SERVER_URL}")
        return False

def check_command_history(device_id: str = "EcoWatt001"):
    """Check command execution history."""
    
    endpoint = f"{SERVER_URL}/api/cloud/command/history"
    
    try:
        response = requests.get(endpoint, params={"device_id": device_id}, timeout=10)
        response.raise_for_status()
        
        history = response.json()
        print("\n📋 Command History:")
        print("-" * 60)
        
        if not history:
            print("   No commands in history")
            return
            
        for i, cmd in enumerate(history[-5:], 1):  # Show last 5
            print(f"   {i}. [{cmd.get('status', 'unknown')}] "
                  f"Register {cmd.get('target_register')} = {cmd.get('value')} "
                  f"@ {cmd.get('timestamp', 'N/A')}")
        
    except requests.exceptions.RequestException as e:
        print(f"\n❌ Error fetching history: {e}")

def main():
    parser = argparse.ArgumentParser(
        description="Send a command to the ESP32 device (Feature 4 Demo)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python send_command.py --register 8 --value 75
      Set Battery SOC to 75%
      
  python send_command.py --register 9 --value 1500
      Set Output Power to 1500W
      
  python send_command.py --history
      Check command execution history
        """
    )
    
    parser.add_argument("--register", "-r", type=int, 
                        help="Register number to write (e.g., 8 for Battery SOC)")
    parser.add_argument("--value", "-v", type=float,
                        help="Value to write to the register")
    parser.add_argument("--device", "-d", type=str, default="EcoWatt001",
                        help="Device ID (default: EcoWatt001)")
    parser.add_argument("--encrypted", "-e", action="store_true",
                        help="Use encrypted command payload")
    parser.add_argument("--history", "-H", action="store_true",
                        help="Show command execution history")
    
    args = parser.parse_args()
    
    if args.history:
        check_command_history(args.device)
        return 0
    
    if args.register is None or args.value is None:
        # Default demo: Set Battery SOC to 85%
        print("No arguments provided. Running default demo...")
        print("Setting Battery SOC (register 8) to 66%\n")
        args.register = 8
        args.value = 66
    
    success = send_command(
        register=args.register,
        value=args.value,
        device_id=args.device,
        encrypted=args.encrypted
    )
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
