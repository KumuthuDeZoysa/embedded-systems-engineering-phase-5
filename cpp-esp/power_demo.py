#!/usr/bin/env python3
"""
Power Mode Demo Script - Sends commands to ESP32 via Serial
Usage: python power_demo.py [command]

Commands:
  HELP     - Show available commands
  STATS    - Show power statistics  
  HIGH     - Set HIGH_PERFORMANCE mode (240MHz, ~160mA)
  NORMAL   - Set NORMAL mode (160MHz, WiFi sleep, ~20mA)
  LOW      - Set LOW_POWER mode (80MHz, ~15mA)
  COMPARE  - Run automatic comparison demo
"""

import serial
import serial.tools.list_ports
import time
import sys

def find_esp32_port():
    """Find the ESP32 COM port."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if 'CP210' in port.description or 'CH340' in port.description or 'USB' in port.description:
            return port.device
    # Default to COM5 if not found
    return 'COM5'

def send_command(cmd, port=None, wait_time=3):
    """Send a command to ESP32 and read response."""
    if port is None:
        port = find_esp32_port()
    
    print(f"Connecting to {port}...")
    
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(0.5)  # Wait for connection
        
        # Clear any pending data
        ser.reset_input_buffer()
        
        # Send command
        cmd_bytes = (cmd + '\n').encode('utf-8')
        print(f"Sending: {cmd}")
        ser.write(cmd_bytes)
        
        # Read response for a few seconds
        print(f"\n--- Response ---")
        start_time = time.time()
        while time.time() - start_time < wait_time:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(line)
            time.sleep(0.1)
        
        print("--- End ---\n")
        ser.close()
        return True
        
    except serial.SerialException as e:
        print(f"Error: {e}")
        print("Make sure PlatformIO Serial Monitor is CLOSED first!")
        return False

def interactive_mode(port=None):
    """Interactive mode - type commands."""
    if port is None:
        port = find_esp32_port()
    
    print(f"Interactive Power Demo - {port}")
    print("=" * 50)
    print("Commands: HELP, STATS, POWER HIGH, POWER NORMAL, POWER LOW, COMPARE")
    print("Type 'quit' to exit")
    print("=" * 50)
    
    try:
        ser = serial.Serial(port, 115200, timeout=0.1)
        time.sleep(0.5)
        
        import threading
        running = True
        
        def read_serial():
            while running:
                try:
                    if ser.in_waiting:
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        if line:
                            print(line)
                except:
                    pass
                time.sleep(0.05)
        
        # Start reader thread
        reader = threading.Thread(target=read_serial, daemon=True)
        reader.start()
        
        while True:
            try:
                cmd = input("> ").strip()
                if cmd.lower() == 'quit':
                    break
                if cmd:
                    ser.write((cmd + '\n').encode('utf-8'))
            except KeyboardInterrupt:
                break
        
        running = False
        ser.close()
        
    except serial.SerialException as e:
        print(f"Error: {e}")
        print("Close PlatformIO Serial Monitor first!")

def main():
    if len(sys.argv) < 2:
        # No arguments - run interactive mode
        interactive_mode()
    else:
        cmd = ' '.join(sys.argv[1:]).upper()
        
        # Map shortcuts
        if cmd == 'HIGH':
            cmd = 'POWER HIGH'
        elif cmd == 'NORMAL':
            cmd = 'POWER NORMAL'
        elif cmd == 'LOW':
            cmd = 'POWER LOW'
        
        send_command(cmd)

if __name__ == '__main__':
    main()
