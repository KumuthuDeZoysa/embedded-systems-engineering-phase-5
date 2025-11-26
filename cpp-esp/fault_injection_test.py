#!/usr/bin/env python3
"""
Fault Injection Test Tool for EcoWatt Device
Milestone 5 Part 2: Fault Recovery

This script triggers various fault scenarios using the Inverter SIM API
to test the EcoWatt Device's fault handling and recovery capabilities.
"""

import requests
import time
import json
import argparse
from typing import Dict, Optional

# API Configuration
API_BASE_URL = "http://20.15.114.131:8080"
API_KEY = "your-api-key-here"  # Replace with actual key

class FaultInjector:
    """Fault injection test tool"""
    
    def __init__(self, api_key: str, base_url: str = API_BASE_URL):
        self.api_key = api_key
        self.base_url = base_url
        self.headers = {
            "Authorization": api_key,
            "Content-Type": "application/json",
            "accept": "*/*"
        }
    
    def inject_exception(self, exception_code: int = 1) -> bool:
        """
        Inject a Modbus exception into the next request
        
        Exception Codes:
        1 = Illegal Function
        2 = Illegal Data Address
        3 = Illegal Data Value
        4 = Slave Device Failure
        5 = Acknowledge
        6 = Slave Device Busy
        8 = Memory Parity Error
        """
        print(f"\n🔥 Injecting EXCEPTION fault (code={exception_code})")
        
        payload = {
            "errorType": "EXCEPTION",
            "exceptionCode": exception_code,
            "delayMs": 0
        }
        
        return self._set_error_flag(payload)
    
    def inject_crc_error(self) -> bool:
        """Inject a CRC error into the next response"""
        print(f"\n🔥 Injecting CRC_ERROR fault")
        
        payload = {
            "errorType": "CRC_ERROR",
            "exceptionCode": 0,
            "delayMs": 0
        }
        
        return self._set_error_flag(payload)
    
    def inject_corrupt_response(self) -> bool:
        """Inject corrupt/malformed data into the next response"""
        print(f"\n🔥 Injecting CORRUPT fault")
        
        payload = {
            "errorType": "CORRUPT",
            "exceptionCode": 0,
            "delayMs": 0
        }
        
        return self._set_error_flag(payload)
    
    def inject_packet_drop(self) -> bool:
        """Drop the next packet (simulate timeout)"""
        print(f"\n🔥 Injecting PACKET_DROP fault (timeout simulation)")
        
        payload = {
            "errorType": "PACKET_DROP",
            "exceptionCode": 0,
            "delayMs": 0
        }
        
        return self._set_error_flag(payload)
    
    def inject_delay(self, delay_ms: int = 10000) -> bool:
        """Inject a delay into the next response"""
        print(f"\n🔥 Injecting DELAY fault ({delay_ms}ms)")
        
        payload = {
            "errorType": "DELAY",
            "exceptionCode": 0,
            "delayMs": delay_ms
        }
        
        return self._set_error_flag(payload)
    
    def _set_error_flag(self, payload: Dict) -> bool:
        """Set error flag using the Add Error Flag API"""
        url = f"{self.base_url}/api/user/error-flag/add"
        
        try:
            response = requests.post(url, headers=self.headers, json=payload, timeout=5)
            
            if response.status_code == 200:
                print(f"✓ Fault flag set successfully")
                print(f"  Next request from EcoWatt Device will trigger: {payload['errorType']}")
                return True
            else:
                print(f"✗ Failed to set fault flag: HTTP {response.status_code}")
                print(f"  Response: {response.text}")
                return False
                
        except requests.exceptions.RequestException as e:
            print(f"✗ Request failed: {e}")
            return False
    
    def test_error_emulation(self, slave_address: int = 17, function_code: int = 3,
                            error_type: str = "EXCEPTION", exception_code: int = 1,
                            delay_ms: int = 0) -> Optional[str]:
        """
        Test error emulation API (direct error simulation without flag)
        
        This API immediately returns an error frame without affecting
        the actual Inverter SIM state.
        """
        print(f"\n🧪 Testing Error Emulation API: {error_type}")
        
        url = f"{self.base_url}/api/inverter/error"
        
        payload = {
            "slaveAddress": slave_address,
            "functionCode": function_code,
            "errorType": error_type,
            "exceptionCode": exception_code,
            "delayMs": delay_ms
        }
        
        # Note: No authorization header needed for this API
        headers = {"Content-Type": "application/json", "accept": "*/*"}
        
        try:
            response = requests.post(url, headers=headers, json=payload, timeout=5)
            
            if response.status_code == 200:
                result = response.json()
                print(f"✓ Error frame generated: {result.get('frame', 'N/A')}")
                return result.get('frame')
            else:
                print(f"✗ Failed: HTTP {response.status_code}")
                print(f"  Response: {response.text}")
                return None
                
        except requests.exceptions.RequestException as e:
            print(f"✗ Request failed: {e}")
            return None


def run_fault_test_sequence(injector: FaultInjector, interval_seconds: int = 30):
    """
    Run a comprehensive fault injection test sequence
    
    This injects various faults at intervals, allowing the EcoWatt Device
    to encounter and recover from each fault type.
    """
    print("=" * 70)
    print("🔬 FAULT INJECTION TEST SEQUENCE")
    print("=" * 70)
    print(f"\nThis will inject faults at {interval_seconds}-second intervals")
    print("Make sure your EcoWatt Device is running and monitoring serial output!")
    print("\nPress Ctrl+C to stop\n")
    
    test_sequence = [
        ("EXCEPTION (Illegal Function)", lambda: injector.inject_exception(1)),
        ("EXCEPTION (Illegal Address)", lambda: injector.inject_exception(2)),
        ("CRC_ERROR", lambda: injector.inject_crc_error()),
        ("CORRUPT Response", lambda: injector.inject_corrupt_response()),
        ("PACKET_DROP (Timeout)", lambda: injector.inject_packet_drop()),
        ("DELAY (10s)", lambda: injector.inject_delay(10000)),
        ("DELAY (5s)", lambda: injector.inject_delay(5000)),
    ]
    
    try:
        for i, (name, inject_func) in enumerate(test_sequence):
            print(f"\n[Test {i+1}/{len(test_sequence)}] {name}")
            print("-" * 70)
            
            success = inject_func()
            
            if success:
                print(f"⏳ Waiting {interval_seconds} seconds for device to trigger fault...")
                time.sleep(interval_seconds)
            else:
                print(f"⚠ Fault injection failed, skipping wait period")
                time.sleep(5)
        
        print("\n" + "=" * 70)
        print("✓ Fault injection sequence complete!")
        print("=" * 70)
        print("\nCheck your EcoWatt Device logs and event log for recovery results.")
        
    except KeyboardInterrupt:
        print("\n\n⚠ Test sequence interrupted by user")


def main():
    parser = argparse.ArgumentParser(
        description="Fault Injection Tool for EcoWatt Device Testing"
    )
    
    parser.add_argument(
        "--api-key",
        type=str,
        required=True,
        help="API key for Inverter SIM service"
    )
    
    parser.add_argument(
        "--base-url",
        type=str,
        default=API_BASE_URL,
        help=f"Base URL for API (default: {API_BASE_URL})"
    )
    
    parser.add_argument(
        "--mode",
        choices=["single", "sequence", "emulate"],
        default="sequence",
        help="Test mode: single fault, sequence, or emulation"
    )
    
    parser.add_argument(
        "--fault-type",
        choices=["EXCEPTION", "CRC_ERROR", "CORRUPT", "PACKET_DROP", "DELAY"],
        default="EXCEPTION",
        help="Fault type for single mode (default: EXCEPTION)"
    )
    
    parser.add_argument(
        "--exception-code",
        type=int,
        default=1,
        help="Exception code (1-8) for EXCEPTION faults (default: 1)"
    )
    
    parser.add_argument(
        "--delay-ms",
        type=int,
        default=10000,
        help="Delay in milliseconds for DELAY faults (default: 10000)"
    )
    
    parser.add_argument(
        "--interval",
        type=int,
        default=30,
        help="Interval in seconds between faults in sequence mode (default: 30)"
    )
    
    args = parser.parse_args()
    
    print("=" * 70)
    print("EcoWatt Device - Fault Injection Test Tool")
    print("Milestone 5 Part 2: Fault Recovery")
    print("=" * 70)
    
    injector = FaultInjector(args.api_key, args.base_url)
    
    if args.mode == "single":
        print(f"\n📍 Single Fault Mode: {args.fault_type}")
        
        if args.fault_type == "EXCEPTION":
            injector.inject_exception(args.exception_code)
        elif args.fault_type == "CRC_ERROR":
            injector.inject_crc_error()
        elif args.fault_type == "CORRUPT":
            injector.inject_corrupt_response()
        elif args.fault_type == "PACKET_DROP":
            injector.inject_packet_drop()
        elif args.fault_type == "DELAY":
            injector.inject_delay(args.delay_ms)
            
    elif args.mode == "sequence":
        run_fault_test_sequence(injector, args.interval)
        
    elif args.mode == "emulate":
        print(f"\n🧪 Emulation Mode: Testing Error Emulation API")
        injector.test_error_emulation(
            error_type=args.fault_type,
            exception_code=args.exception_code,
            delay_ms=args.delay_ms
        )


if __name__ == "__main__":
    main()
