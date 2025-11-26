#!/usr/bin/env python3
"""
Event Log Monitor for EcoWatt Device
Milestone 5 Part 2: Fault Recovery

Monitors serial output from EcoWatt Device and tracks fault/recovery events.
"""

import serial
import time
import json
import argparse
import re
from datetime import datetime
from typing import List, Dict

class EventLogMonitor:
    """Monitor and analyze event logs from EcoWatt Device"""
    
    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.events = []
        
    def connect(self) -> bool:
        """Connect to serial port"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"✓ Connected to {self.port} at {self.baudrate} baud")
            time.sleep(2)  # Wait for connection to stabilize
            return True
        except serial.SerialException as e:
            print(f"✗ Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from serial port"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("✓ Disconnected")
    
    def monitor(self, duration_seconds: int = 300):
        """
        Monitor serial output for event logs
        
        Args:
            duration_seconds: How long to monitor (0 = forever)
        """
        if not self.ser or not self.ser.is_open:
            print("✗ Not connected to serial port")
            return
        
        print("\n" + "=" * 70)
        print("📊 Event Log Monitoring")
        print("=" * 70)
        print(f"Duration: {duration_seconds}s" if duration_seconds > 0 else "Duration: Continuous (Ctrl+C to stop)")
        print("=" * 70 + "\n")
        
        start_time = time.time()
        event_count = 0
        fault_count = 0
        recovery_count = 0
        
        try:
            while True:
                # Check duration
                if duration_seconds > 0:
                    elapsed = time.time() - start_time
                    if elapsed >= duration_seconds:
                        break
                
                # Read line
                if self.ser.in_waiting > 0:
                    try:
                        line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                        
                        if not line:
                            continue
                        
                        # Check for event log entries
                        if "[EventLog]" in line:
                            event_count += 1
                            self._process_event_line(line)
                            
                            if "FAULT" in line:
                                fault_count += 1
                                print(f"  🔴 Fault #{fault_count}")
                            elif "RECOVERY" in line:
                                recovery_count += 1
                                print(f"  🟢 Recovery #{recovery_count}")
                        
                        # Check for fault handler messages
                        elif "[FaultHandler]" in line:
                            print(f"  ⚙️  {line}")
                        
                        # Show all relevant lines
                        elif any(x in line for x in ["[ERROR]", "[WARN]", "[PowerMgr]", "[FOTA]"]):
                            print(f"  📋 {line}")
                    
                    except UnicodeDecodeError:
                        continue
                
                time.sleep(0.01)
        
        except KeyboardInterrupt:
            print("\n\n⚠ Monitoring stopped by user")
        
        finally:
            print("\n" + "=" * 70)
            print("📈 Monitoring Summary")
            print("=" * 70)
            print(f"  Total Events: {event_count}")
            print(f"  Faults: {fault_count}")
            print(f"  Recoveries: {recovery_count}")
            
            if fault_count > 0:
                recovery_rate = (recovery_count / fault_count) * 100
                print(f"  Recovery Rate: {recovery_rate:.1f}%")
            
            print("=" * 70)
    
    def _process_event_line(self, line: str):
        """Process and store event log line"""
        # Parse event log format: [EventLog] [module] [type] description (RECOVERED)
        
        # Extract components using regex
        match = re.search(r'\[EventLog\]\s+\[(\w+)\]\s+\[(\w+)\]\s+(.+)', line)
        
        if match:
            module = match.group(1)
            event_type = match.group(2)
            description = match.group(3)
            recovered = "(RECOVERED)" in description
            
            event = {
                "timestamp": datetime.now().isoformat(),
                "module": module,
                "type": event_type,
                "description": description.replace("(RECOVERED)", "").strip(),
                "recovered": recovered
            }
            
            self.events.append(event)
            
            # Display with color
            color_code = {
                "INFO": "37",      # White
                "WARNING": "33",   # Yellow
                "ERROR": "31",     # Red
                "FAULT": "91",     # Bright Red
                "RECOVERY": "32"   # Green
            }.get(event_type, "37")
            
            print(f"\033[{color_code}m  [{event_type}] [{module}] {description}\033[0m")
    
    def export_events(self, filename: str = "event_log_export.json"):
        """Export collected events to JSON file"""
        with open(filename, 'w') as f:
            json.dump(self.events, f, indent=2)
        
        print(f"\n✓ Exported {len(self.events)} events to {filename}")
    
    def generate_report(self) -> str:
        """Generate markdown report of events"""
        if not self.events:
            return "No events collected."
        
        report = "# Event Log Report\n\n"
        report += f"**Generated**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n"
        report += f"**Total Events**: {len(self.events)}\n\n"
        
        # Group by module
        by_module = {}
        by_type = {}
        
        for event in self.events:
            module = event['module']
            event_type = event['type']
            
            if module not in by_module:
                by_module[module] = []
            by_module[module].append(event)
            
            if event_type not in by_type:
                by_type[event_type] = 0
            by_type[event_type] += 1
        
        report += "## Event Distribution by Type\n\n"
        report += "| Type | Count |\n"
        report += "|------|-------|\n"
        for event_type, count in sorted(by_type.items()):
            report += f"| {event_type} | {count} |\n"
        
        report += "\n## Event Distribution by Module\n\n"
        report += "| Module | Count |\n"
        report += "|--------|-------|\n"
        for module, events in sorted(by_module.items()):
            report += f"| {module} | {len(events)} |\n"
        
        # Fault/Recovery Analysis
        faults = [e for e in self.events if e['type'] == 'FAULT']
        recoveries = [e for e in self.events if e['type'] == 'RECOVERY' or e.get('recovered')]
        
        report += "\n## Fault Recovery Analysis\n\n"
        report += f"- **Total Faults**: {len(faults)}\n"
        report += f"- **Successful Recoveries**: {len(recoveries)}\n"
        
        if len(faults) > 0:
            recovery_rate = (len(recoveries) / len(faults)) * 100
            report += f"- **Recovery Rate**: {recovery_rate:.1f}%\n"
        
        report += "\n## Detailed Event Log\n\n"
        report += "| Timestamp | Module | Type | Description |\n"
        report += "|-----------|--------|------|-------------|\n"
        
        for event in self.events:
            report += f"| {event['timestamp']} | {event['module']} | {event['type']} | {event['description']} |\n"
        
        return report
    
    def save_report(self, filename: str = "event_log_report.md"):
        """Save markdown report to file"""
        report = self.generate_report()
        
        with open(filename, 'w') as f:
            f.write(report)
        
        print(f"✓ Report saved to {filename}")


def main():
    parser = argparse.ArgumentParser(
        description="Event Log Monitor for EcoWatt Device"
    )
    
    parser.add_argument(
        "--port",
        type=str,
        default="COM5",
        help="Serial port (default: COM5)"
    )
    
    parser.add_argument(
        "--baudrate",
        type=int,
        default=115200,
        help="Baud rate (default: 115200)"
    )
    
    parser.add_argument(
        "--duration",
        type=int,
        default=300,
        help="Monitoring duration in seconds (0 = continuous, default: 300)"
    )
    
    parser.add_argument(
        "--export",
        type=str,
        help="Export events to JSON file"
    )
    
    parser.add_argument(
        "--report",
        type=str,
        help="Generate markdown report"
    )
    
    args = parser.parse_args()
    
    print("=" * 70)
    print("Event Log Monitor - EcoWatt Device")
    print("Milestone 5 Part 2: Fault Recovery")
    print("=" * 70)
    
    monitor = EventLogMonitor(args.port, args.baudrate)
    
    if not monitor.connect():
        return
    
    try:
        monitor.monitor(args.duration)
        
        if args.export:
            monitor.export_events(args.export)
        
        if args.report:
            monitor.save_report(args.report)
    
    finally:
        monitor.disconnect()


if __name__ == "__main__":
    main()
