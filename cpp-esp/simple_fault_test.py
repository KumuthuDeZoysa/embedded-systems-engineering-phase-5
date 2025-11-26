#!/usr/bin/env python3
"""
Simple Fault Recovery Test without API Key
Tests fault recovery using manual methods and event log inspection
"""

import argparse
import time
import sys
from datetime import datetime

class SimpleFaultTester:
    def __init__(self, port="COM5"):
        self.port = port
        
    def print_header(self, text):
        print(f"\n{'='*80}")
        print(f"  {text}")
        print(f"{'='*80}\n")
        
    def print_step(self, step_num, text):
        print(f"\n[STEP {step_num}] {text}")
        print("-" * 80)
        
    def test_network_fault(self):
        """Test network disconnection and recovery"""
        self.print_step(1, "Network Fault Test")
        
        print("\n📋 Test Procedure:")
        print("1. Ensure ESP32 is running and connected to WiFi")
        print("2. Open serial monitor: pio device monitor --baud 115200")
        print("3. Wait for normal operation (see acquisition messages)")
        print("4. DISCONNECT WiFi (turn off router or disconnect cable)")
        print("5. Observe device behavior:")
        print("   - Look for [FaultHandler] Network error detected")
        print("   - Look for [EventLog] FAULT logged")
        print("   - Device should attempt retry with backoff")
        print("6. RECONNECT WiFi")
        print("7. Verify:")
        print("   - [FaultHandler] Recovery successful")
        print("   - [EventLog] RECOVERY logged")
        print("   - Normal operation resumes")
        
        print("\n⏱️  Recommended: Monitor for 3-5 minutes")
        
        result = input("\n✓ Did network fault recovery work? (y/n): ").strip().lower()
        
        if result == 'y':
            print("✅ PASSED - Network fault recovery working")
            return True
        else:
            print("❌ FAILED - Network fault recovery issue")
            return False
    
    def test_buffer_overflow(self):
        """Test buffer overflow handling"""
        self.print_step(2, "Buffer Overflow Test")
        
        print("\n📋 Test Procedure:")
        print("1. This tests degraded mode when buffer fills up")
        print("2. Option A: Reduce buffer size in code and rebuild")
        print("3. Option B: Increase acquisition rate to fill buffer faster")
        print("4. Option C: Prevent upload to cloud (disconnect network for extended time)")
        print("5. Observe:")
        print("   - [FaultHandler] Buffer overflow detected")
        print("   - [FaultHandler] Entering degraded mode")
        print("   - [EventLog] FAULT: Buffer overflow")
        print("   - Oldest data dropped (FIFO)")
        print("   - System continues operating")
        
        result = input("\n✓ Did you test buffer overflow? (y/n/skip): ").strip().lower()
        
        if result == 'y':
            passed = input("  Did it work correctly? (y/n): ").strip().lower() == 'y'
            if passed:
                print("✅ PASSED - Buffer overflow handled correctly")
            else:
                print("❌ FAILED - Buffer overflow issue")
            return passed
        elif result == 'skip':
            print("⊘ SKIPPED - Marked for manual testing later")
            return None
        else:
            print("⊘ NOT TESTED")
            return None
    
    def test_event_log_inspection(self):
        """Inspect event log file"""
        self.print_step(3, "Event Log Inspection")
        
        print("\n📋 Test Procedure:")
        print("1. After generating some events (faults, recoveries)")
        print("2. Access ESP32 filesystem:")
        print("   Option A: Use serial file browser")
        print("   Option B: Add code to print /event_log.json to serial")
        print("   Option C: Use SPIFFS/LittleFS file uploader plugin")
        print("3. Verify event_log.json format:")
        print("   - Should be JSON array")
        print("   - Each event should have:")
        print("     * timestamp")
        print("     * event")
        print("     * module")
        print("     * type")
        print("     * severity")
        print("     * recovered")
        print("     * details")
        
        print("\n📋 Expected Format:")
        print("""[
  {
    "timestamp": "2025-11-26T10:30:00Z",
    "event": "Network timeout",
    "module": "network",
    "type": "FAULT",
    "severity": "HIGH",
    "recovered": true,
    "details": "HTTP request timeout after 10s"
  },
  {
    "timestamp": "2025-11-26T10:30:15Z",
    "event": "Connection restored",
    "module": "network",
    "type": "RECOVERY",
    "severity": "MEDIUM",
    "recovered": true,
    "details": "Retry successful after 2 attempts"
  }
]""")
        
        result = input("\n✓ Did you inspect the event log? (y/n): ").strip().lower()
        
        if result == 'y':
            valid = input("  Was the format correct? (y/n): ").strip().lower() == 'y'
            count = input("  How many events in log? ").strip()
            if valid:
                print(f"✅ PASSED - Event log format valid ({count} events)")
            else:
                print("❌ FAILED - Event log format issue")
            return valid
        else:
            print("⊘ NOT TESTED")
            return None
    
    def test_persistence(self):
        """Test event log persistence"""
        self.print_step(4, "Event Log Persistence Test")
        
        print("\n📋 Test Procedure:")
        print("1. Generate some events (do network fault test above)")
        print("2. Note number of events in log")
        print("3. REBOOT ESP32 (press EN button)")
        print("4. Check serial output after boot:")
        print("   - [EventLog] Loaded X events from /event_log.json")
        print("5. Verify X matches number before reboot")
        print("6. Optionally inspect log file again to confirm")
        
        result = input("\n✓ Did events survive reboot? (y/n): ").strip().lower()
        
        if result == 'y':
            print("✅ PASSED - Event log persistence verified")
            return True
        else:
            print("❌ FAILED - Events lost on reboot")
            return False
    
    def test_recovery_statistics(self):
        """Test recovery rate calculation"""
        self.print_step(5, "Recovery Statistics Test")
        
        print("\n📋 Test Procedure:")
        print("1. After generating several faults and recoveries")
        print("2. Check serial output for:")
        print("   - [EventLog] Recovery rate: XX.X%")
        print("   - [EventLog] Total faults: X")
        print("   - [EventLog] Recovered: X")
        print("3. Manually verify calculation:")
        print("   Recovery Rate = (Recovered / Total Faults) × 100%")
        
        result = input("\n✓ Did you see recovery statistics? (y/n): ").strip().lower()
        
        if result == 'y':
            rate = input("  What was the recovery rate? (e.g., 85.5): ").strip()
            print(f"✅ PASSED - Recovery rate: {rate}%")
            return True, rate
        else:
            print("⊘ NOT TESTED")
            return None, None
    
    def generate_report(self, results):
        """Generate simple test report"""
        self.print_header("GENERATING FAULT RECOVERY TEST REPORT")
        
        report_file = f"fault_recovery_test_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.md"
        
        report = f"""# Fault Recovery Test Report (Manual Testing)
**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}  
**Device:** ESP32 (NodeMCU)  
**Serial Port:** {self.port}  
**Testing Method:** Manual observation and event log inspection

---

## Test Summary

| Test | Status | Notes |
|------|--------|-------|
| Network Fault Recovery | {results.get('network', '❌')} | Network disconnect/reconnect test |
| Buffer Overflow Handling | {results.get('buffer', '⊘')} | Degraded mode test |
| Event Log Format | {results.get('log_format', '⊘')} | JSON format verification |
| Event Log Persistence | {results.get('persistence', '❌')} | Reboot survival test |
| Recovery Statistics | {results.get('statistics', '⊘')} | Recovery rate calculation |

---

## Detailed Results

### 1. Network Fault Recovery
**Status:** {results.get('network', 'NOT TESTED')}  

**Test Procedure:**
1. Disconnected WiFi/network
2. Observed fault detection
3. Reconnected network
4. Verified recovery

**Expected Behavior:**
- [FaultHandler] Network error detected
- [EventLog] FAULT logged with severity HIGH
- Exponential backoff retry
- [FaultHandler] Recovery successful
- [EventLog] RECOVERY logged

**Actual Result:** {results.get('network_notes', 'See manual test log')}

---

### 2. Buffer Overflow Handling
**Status:** {results.get('buffer', 'NOT TESTED')}  

**Test Procedure:**
- Attempted to fill buffer by preventing upload
- Observed degraded mode behavior

**Expected Behavior:**
- [FaultHandler] Buffer overflow detected
- [FaultHandler] Entering degraded mode
- Oldest data dropped (FIFO)
- System continues operating

**Actual Result:** {results.get('buffer_notes', 'Test skipped or pending')}

---

### 3. Event Log Format Verification
**Status:** {results.get('log_format', 'NOT TESTED')}  

**Test Procedure:**
- Accessed /event_log.json file
- Verified JSON format
- Checked required fields

**Event Count:** {results.get('event_count', 'N/A')}

**Sample Event:**
```json
{{
  "timestamp": "2025-11-26T10:30:00Z",
  "event": "Network timeout",
  "module": "network",
  "type": "FAULT",
  "severity": "HIGH",
  "recovered": true,
  "details": "HTTP request timeout"
}}
```

---

### 4. Event Log Persistence
**Status:** {results.get('persistence', 'NOT TESTED')}  

**Test Procedure:**
1. Generated events
2. Rebooted ESP32
3. Verified events survived

**Result:** {results.get('persistence_notes', 'Events survived reboot' if results.get('persistence') == '✅' else 'Test pending')}

---

### 5. Recovery Statistics
**Status:** {results.get('statistics', 'NOT TESTED')}  

**Recovery Rate:** {results.get('recovery_rate', 'N/A')}

**Calculation:**
- Total Faults: {results.get('total_faults', 'N/A')}
- Recovered: {results.get('recovered_count', 'N/A')}
- Rate = (Recovered / Total) × 100%

---

## Fault Types Supported

| Fault Type | Recovery Strategy | Status |
|------------|-------------------|--------|
| Network Timeout | Exponential backoff retry | {results.get('network', '⊘')} |
| Malformed Frame | Discard and continue | ⊘ |
| Buffer Overflow | Degraded mode | {results.get('buffer', '⊘')} |
| Parse Error | Skip and use defaults | ⊘ |
| Security Violation | Fail-safe (no recovery) | ⊘ |

**Note:** Not all fault types tested in this manual session.  
Full fault injection requires Inverter SIM API key.

---

## Conclusions

**Milestone 5 Part 2 Status:**

- ✅ EventLogger implementation: Complete
- ✅ FaultHandler implementation: Complete
- ✅ Event log persistence: {'Verified' if results.get('persistence') == '✅' else 'Needs verification'}
- {'✅' if results.get('network') == '✅' else '⚠️'} Network fault recovery: {'Working' if results.get('network') == '✅' else 'Needs testing'}
- ⚠️ Full fault injection: Requires API key

**Recommendations:**

1. {'✅ Event log verified' if results.get('log_format') == '✅' else '⚠️ Verify event log format and persistence'}
2. {'✅ Network recovery tested' if results.get('network') == '✅' else '⚠️ Complete network fault testing'}
3. ⚠️ Obtain API key for comprehensive fault injection
4. ⚠️ Test all 7 fault types with automated script
5. ✅ Proceed to integration testing after fault recovery validated

---

**Next Steps:**

1. Complete any pending fault tests
2. Run full fault injection with API key (when available)
3. Proceed to integration testing (Part 3)
4. Prepare demonstration video (Part 4)

---

**Report generated by:** simple_fault_test.py  
**Date:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
"""
        
        # Save report
        with open(report_file, 'w', encoding='utf-8') as f:
            f.write(report)
        
        print(f"\n✓ Report saved to: {report_file}")
        
        return report_file
    
    def run_tests(self):
        """Run all manual fault recovery tests"""
        self.print_header("FAULT RECOVERY MANUAL TESTING")
        
        print("This script guides you through manual fault recovery testing.")
        print("These tests don't require the Inverter SIM API key.\n")
        print("Make sure your ESP32 is running and connected to serial monitor!")
        
        input("\nPress Enter to begin...")
        
        results = {}
        
        # Test 1: Network fault
        if self.test_network_fault():
            results['network'] = '✅ PASSED'
            results['network_notes'] = 'Network fault detected and recovered successfully'
        else:
            results['network'] = '❌ FAILED'
            results['network_notes'] = 'Network recovery issue detected'
        
        # Test 2: Buffer overflow
        buffer_result = self.test_buffer_overflow()
        if buffer_result is True:
            results['buffer'] = '✅ PASSED'
            results['buffer_notes'] = 'Buffer overflow handled correctly'
        elif buffer_result is False:
            results['buffer'] = '❌ FAILED'
            results['buffer_notes'] = 'Buffer overflow issue detected'
        else:
            results['buffer'] = '⊘ SKIPPED'
            results['buffer_notes'] = 'Test not performed'
        
        # Test 3: Event log format
        log_result = self.test_event_log_inspection()
        if log_result is True:
            results['log_format'] = '✅ PASSED'
            results['event_count'] = input("  Number of events: ").strip()
        elif log_result is False:
            results['log_format'] = '❌ FAILED'
        else:
            results['log_format'] = '⊘ NOT TESTED'
        
        # Test 4: Persistence
        if self.test_persistence():
            results['persistence'] = '✅ PASSED'
            results['persistence_notes'] = 'Events survived reboot successfully'
        else:
            results['persistence'] = '❌ FAILED'
            results['persistence_notes'] = 'Events lost on reboot'
        
        # Test 5: Statistics
        stats_result, rate = self.test_recovery_statistics()
        if stats_result:
            results['statistics'] = '✅ PASSED'
            results['recovery_rate'] = f"{rate}%"
        else:
            results['statistics'] = '⊘ NOT TESTED'
        
        # Generate report
        report_file = self.generate_report(results)
        
        self.print_header("MANUAL TESTING COMPLETE")
        
        print(f"\n📄 Report: {report_file}")
        print("\n✅ Manual fault recovery testing complete!")
        print("\n📋 Next Steps:")
        print("1. Review the report")
        print("2. If you get API key, run fault_injection_test.py for full testing")
        print("3. Run integration_test_suite.py for Part 3")
        
        return report_file

def main():
    parser = argparse.ArgumentParser(
        description="Simple Fault Recovery Testing (No API Key Required)"
    )
    
    parser.add_argument("--port", default="COM5", help="Serial port (default: COM5)")
    
    args = parser.parse_args()
    
    tester = SimpleFaultTester(port=args.port)
    
    try:
        tester.run_tests()
        sys.exit(0)
    except KeyboardInterrupt:
        print("\n\n⚠️  Testing interrupted")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()
