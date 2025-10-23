"""
Interactive Serial Monitor with Pause/Resume for Demo
Press SPACE to pause/resume, F to filter, Q to quit
"""

import serial
import sys
import msvcrt
from colorama import init, Fore, Style
from datetime import datetime

init()

SERIAL_PORT = "COM5"
BAUD_RATE = 115200

class DemoSerialMonitor:
    def __init__(self, port, baudrate):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.paused = False
        self.filter_enabled = True
        self.buffer = []
        
        # Filter keywords
        self.filter_out = ["[DEBUG]", "Heap:", "Free:", "WiFi rssi:"]
        
    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            print(f"{Fore.GREEN}✓ Connected to {self.port}{Style.RESET_ALL}\n")
            return True
        except serial.SerialException as e:
            print(f"{Fore.RED}✗ Failed to open {self.port}: {e}{Style.RESET_ALL}")
            return False
    
    def should_filter(self, line):
        if not self.filter_enabled:
            return False
        return any(keyword.lower() in line.lower() for keyword in self.filter_out)
    
    def colorize(self, line):
        """Color code important logs"""
        upper = line.upper()
        
        # Priority colors
        if any(w in upper for w in ["ERROR", "FAILED"]):
            return Fore.RED + Style.BRIGHT + line
        if any(w in upper for w in ["SUCCESS", "✓", "VERIFIED"]):
            return Fore.GREEN + Style.BRIGHT + line
        if any(w in upper for w in ["[CONFIG]", "[COMMAND]", "[FOTA]"]):
            return Fore.CYAN + Style.BRIGHT + line
        if "[SECURITY]" in upper:
            return Fore.MAGENTA + Style.BRIGHT + line
        if any(w in upper for w in ["WARN", "ROLLBACK"]):
            return Fore.YELLOW + Style.BRIGHT + line
        
        return Fore.WHITE + line
    
    def display_status(self):
        """Display current status bar"""
        status = f"\r{Back.BLUE}{Fore.WHITE}"
        status += f" [{datetime.now().strftime('%H:%M:%S')}] "
        status += "PAUSED" if self.paused else "RUNNING"
        status += " | "
        status += f"Filter: {'ON' if self.filter_enabled else 'OFF'}"
        status += " | "
        status += "SPACE=Pause/Resume F=Filter Q=Quit"
        status += f"{Style.RESET_ALL}"
        
        # Clear line and print status
        sys.stdout.write("\033[K")  # Clear to end of line
        sys.stdout.write(status)
        sys.stdout.flush()
    
    def run(self):
        if not self.connect():
            return
        
        print(f"{Fore.CYAN}{'='*70}")
        print(f"  EcoWatt Demo Serial Monitor")
        print(f"  Controls: SPACE=Pause/Resume | F=Toggle Filter | Q=Quit")
        print(f"{'='*70}{Style.RESET_ALL}\n")
        
        try:
            while True:
                # Check for keyboard input
                if msvcrt.kbhit():
                    key = msvcrt.getch()
                    
                    # Space - Pause/Resume
                    if key == b' ':
                        self.paused = not self.paused
                        if self.paused:
                            print(f"\n{Fore.YELLOW}⏸ PAUSED - Press SPACE to resume{Style.RESET_ALL}")
                        else:
                            print(f"{Fore.GREEN}▶ RESUMED{Style.RESET_ALL}\n")
                    
                    # F - Toggle filter
                    elif key.lower() == b'f':
                        self.filter_enabled = not self.filter_enabled
                        status = "enabled" if self.filter_enabled else "disabled"
                        print(f"\n{Fore.CYAN}Filter {status}{Style.RESET_ALL}\n")
                    
                    # Q - Quit
                    elif key.lower() == b'q':
                        print(f"\n{Fore.YELLOW}Quitting...{Style.RESET_ALL}")
                        break
                
                # Read and display logs if not paused
                if not self.paused and self.ser.in_waiting > 0:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if line and not self.should_filter(line):
                        print(self.colorize(line) + Style.RESET_ALL)
                
        except KeyboardInterrupt:
            print(f"\n{Fore.YELLOW}Interrupted{Style.RESET_ALL}")
        finally:
            if self.ser and self.ser.is_open:
                self.ser.close()
            print(f"\n{Fore.GREEN}✓ Disconnected{Style.RESET_ALL}")

if __name__ == "__main__":
    monitor = DemoSerialMonitor(SERIAL_PORT, BAUD_RATE)
    monitor.run()
