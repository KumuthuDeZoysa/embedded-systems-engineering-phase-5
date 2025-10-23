"""
Serial Monitor Log Filter for Video Demo
Filters and highlights important logs for clear demonstration
"""

import serial
import re
import sys
from colorama import init, Fore, Back, Style

# Initialize colorama for Windows color support
init()

# Configure serial port
SERIAL_PORT = "COM5"
BAUD_RATE = 115200

# Keywords to highlight (case-insensitive)
IMPORTANT_KEYWORDS = [
    "CONFIG",
    "COMMAND",
    "FOTA",
    "SECURITY",
    "VERIFICATION",
    "UPDATE",
    "SUCCESS",
    "FAILED",
    "ERROR",
    "ROLLBACK",
    "APPLIED"
]

# Keywords to filter out (reduce noise)
FILTER_OUT = [
    "[DEBUG]",
    "Heap:",
    "Free:",
    "WiFi rssi:",
    "timestamp"
]

def should_filter(line):
    """Check if line should be filtered out"""
    for keyword in FILTER_OUT:
        if keyword.lower() in line.lower():
            return True
    return False

def colorize_line(line):
    """Add color highlighting to important keywords"""
    # Error/Failed - Red
    if any(word in line.upper() for word in ["ERROR", "FAILED", "REJECT"]):
        return Fore.RED + Style.BRIGHT + line + Style.RESET_ALL
    
    # Success/Applied - Green
    if any(word in line.upper() for word in ["SUCCESS", "APPLIED", "VERIFIED", "✓"]):
        return Fore.GREEN + Style.BRIGHT + line + Style.RESET_ALL
    
    # Warning/Rollback - Yellow
    if any(word in line.upper() for word in ["WARN", "ROLLBACK", "RETRY"]):
        return Fore.YELLOW + Style.BRIGHT + line + Style.RESET_ALL
    
    # Config/Command/FOTA - Cyan (important actions)
    if any(word in line.upper() for word in ["[CONFIG]", "[COMMAND]", "[CMD]", "[FOTA]"]):
        return Fore.CYAN + Style.BRIGHT + line + Style.RESET_ALL
    
    # Security - Magenta
    if "[SECURITY]" in line.upper():
        return Fore.MAGENTA + Style.BRIGHT + line + Style.RESET_ALL
    
    # Info - White (default)
    if "[INFO]" in line.upper():
        return Fore.WHITE + line + Style.RESET_ALL
    
    # Default - dim white
    return Style.DIM + line + Style.RESET_ALL

def main():
    print(f"{Fore.CYAN}{'='*60}")
    print(f"  EcoWatt Serial Monitor - Demo Mode")
    print(f"  Port: {SERIAL_PORT} | Baud: {BAUD_RATE}")
    print(f"  Filtering: {', '.join(FILTER_OUT)}")
    print(f"{'='*60}{Style.RESET_ALL}\n")
    
    try:
        # Open serial connection
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"{Fore.GREEN}✓ Connected to {SERIAL_PORT}{Style.RESET_ALL}\n")
        
        while True:
            try:
                # Read line from serial
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if not line:
                        continue
                    
                    # Filter out noise
                    if should_filter(line):
                        continue
                    
                    # Print with color highlighting
                    print(colorize_line(line))
                    
            except KeyboardInterrupt:
                print(f"\n{Fore.YELLOW}Stopping monitor...{Style.RESET_ALL}")
                break
            except Exception as e:
                print(f"{Fore.RED}Error reading serial: {e}{Style.RESET_ALL}")
                continue
                
    except serial.SerialException as e:
        print(f"{Fore.RED}✗ Failed to open {SERIAL_PORT}: {e}{Style.RESET_ALL}")
        print(f"\nMake sure:")
        print(f"  1. ESP32 is connected to {SERIAL_PORT}")
        print(f"  2. No other program is using the port")
        print(f"  3. Driver is installed (Silicon Labs CP210x)")
        sys.exit(1)
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print(f"{Fore.GREEN}✓ Serial port closed{Style.RESET_ALL}")

if __name__ == "__main__":
    main()
