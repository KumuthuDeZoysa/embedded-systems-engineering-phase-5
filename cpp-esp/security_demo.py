#!/usr/bin/env python3
"""
Security Demonstration Script for EcoWatt System
================================================

This script demonstrates:
1. Normal authenticated requests (should succeed)
2. Replay attack simulation (should fail)
3. Invalid HMAC attack (should fail)
4. Missing security headers (should fail)

Usage:
    python security_demo.py

The script will show various attack scenarios and the server's
security responses for video demonstration purposes.
"""

import requests
import hmac
import hashlib
import time
import json

# Configuration
SERVER_URL = "http://10.50.126.183:8080"
DEVICE_ID = "EcoWatt001"
PSK = bytes.fromhex("c41716a134168f52fbd4be3302fa5a88127ddde749501a199607b4c286ad29b3")

def create_hmac_signature(endpoint, nonce, timestamp):
    """Create HMAC-SHA256 signature for request authentication."""
    hmac_input = endpoint + str(nonce) + str(timestamp)
    return hmac.new(PSK, hmac_input.encode(), hashlib.sha256).hexdigest()

def make_authenticated_request(endpoint, nonce, timestamp):
    """Make a properly authenticated request."""
    mac = create_hmac_signature(endpoint, nonce, timestamp)
    headers = {
        'Device-ID': DEVICE_ID,
        'X-Nonce': str(nonce),
        'X-Timestamp': str(timestamp),
        'X-MAC': mac
    }
    return requests.get(f"{SERVER_URL}{endpoint}", headers=headers)

def demo_normal_request():
    """Demonstrate normal authenticated request (should succeed)."""
    print("\\n" + "="*60)
    print("🔒 DEMO 1: Normal Authenticated Request")
    print("="*60)
    
    nonce = int(time.time()) % 10000 + 100  # Use current time-based nonce
    timestamp = int(time.time())
    
    print(f"📤 Sending authenticated request:")
    print(f"   Nonce: {nonce}")
    print(f"   Timestamp: {timestamp}")
    print(f"   Device ID: {DEVICE_ID}")
    
    response = make_authenticated_request("/api/inverter/config", nonce, timestamp)
    
    print(f"📥 Server Response:")
    print(f"   Status Code: {response.status_code}")
    print(f"   Response: {response.text[:200]}...")
    
    if response.status_code == 200:
        print("   ✅ SUCCESS: Request authenticated and processed")
    else:
        print("   ❌ FAILED: Authentication failed")
    
    return nonce, timestamp

def demo_replay_attack(original_nonce, original_timestamp):
    """Demonstrate replay attack (should fail)."""
    print("\\n" + "="*60)
    print("🚨 DEMO 2: Replay Attack Simulation")
    print("="*60)
    
    print(f"📤 Attempting to replay previous request:")
    print(f"   Nonce: {original_nonce} (REUSED - this should fail)")
    print(f"   Timestamp: {original_timestamp}")
    print(f"   Device ID: {DEVICE_ID}")
    
    response = make_authenticated_request("/api/inverter/config", original_nonce, original_timestamp)
    
    print(f"📥 Server Response:")
    print(f"   Status Code: {response.status_code}")
    print(f"   Response: {response.text}")
    
    if response.status_code == 401:
        print("   ✅ SUCCESS: Replay attack detected and blocked!")
    else:
        print("   ❌ UNEXPECTED: Replay attack not detected")

def demo_invalid_hmac():
    """Demonstrate invalid HMAC attack (should fail)."""
    print("\\n" + "="*60)
    print("🔓 DEMO 3: Invalid HMAC Attack")
    print("="*60)
    
    nonce = int(time.time()) % 10000 + 200
    timestamp = int(time.time())
    fake_mac = "deadbeef" * 8  # Obviously fake HMAC
    
    print(f"📤 Sending request with invalid HMAC:")
    print(f"   Nonce: {nonce}")
    print(f"   Timestamp: {timestamp}")
    print(f"   HMAC: {fake_mac} (INVALID)")
    
    headers = {
        'Device-ID': DEVICE_ID,
        'X-Nonce': str(nonce),
        'X-Timestamp': str(timestamp),
        'X-MAC': fake_mac
    }
    
    response = requests.get(f"{SERVER_URL}/api/inverter/config", headers=headers)
    
    print(f"📥 Server Response:")
    print(f"   Status Code: {response.status_code}")
    print(f"   Response: {response.text}")
    
    if response.status_code == 401:
        print("   ✅ SUCCESS: Invalid HMAC detected and blocked!")
    else:
        print("   ❌ UNEXPECTED: Invalid HMAC not detected")

def demo_missing_headers():
    """Demonstrate missing security headers attack (should fail)."""
    print("\\n" + "="*60)
    print("📋 DEMO 4: Missing Security Headers")
    print("="*60)
    
    print(f"📤 Sending request without security headers:")
    print(f"   Device ID: {DEVICE_ID}")
    print(f"   Missing: X-Nonce, X-Timestamp, X-MAC")
    
    headers = {
        'Device-ID': DEVICE_ID
        # Intentionally missing security headers
    }
    
    response = requests.get(f"{SERVER_URL}/api/inverter/config", headers=headers)
    
    print(f"📥 Server Response:")
    print(f"   Status Code: {response.status_code}")
    print(f"   Response: {response.text}")
    
    if response.status_code == 401:
        print("   ✅ SUCCESS: Missing headers detected and blocked!")
    else:
        print("   ❌ UNEXPECTED: Missing headers not detected")

def demo_nonce_window_attack():
    """Demonstrate nonce window boundary attack."""
    print("\\n" + "="*60)
    print("🔢 DEMO 5: Nonce Window Boundary Test")
    print("="*60)
    
    # Try with a very old nonce (outside window)
    old_nonce = 1  # Much lower than current device nonce (~61)
    timestamp = int(time.time())
    
    print(f"📤 Sending request with very old nonce:")
    print(f"   Nonce: {old_nonce} (TOO OLD - outside 100-nonce window)")
    print(f"   Timestamp: {timestamp}")
    
    response = make_authenticated_request("/api/inverter/config", old_nonce, timestamp)
    
    print(f"📥 Server Response:")
    print(f"   Status Code: {response.status_code}")
    print(f"   Response: {response.text}")
    
    if response.status_code == 401:
        print("   ✅ SUCCESS: Old nonce detected and blocked!")
    else:
        print("   ❌ UNEXPECTED: Old nonce accepted")

def main():
    """Run complete security demonstration."""
    print("🛡️  EcoWatt Security Demonstration")
    print("=====================================")
    print("This demonstration shows the security features of the EcoWatt system")
    print("including HMAC authentication, nonce-based replay protection,")
    print("and various attack scenario defenses.")
    
    try:
        # Test 1: Normal authenticated request
        nonce, timestamp = demo_normal_request()
        
        # Small delay between requests
        time.sleep(1)
        
        # Test 2: Replay attack
        demo_replay_attack(nonce, timestamp)
        
        # Test 3: Invalid HMAC
        demo_invalid_hmac()
        
        # Test 4: Missing headers
        demo_missing_headers()
        
        # Test 5: Nonce window test
        demo_nonce_window_attack()
        
        print("\\n" + "="*60)
        print("📊 DEMONSTRATION SUMMARY")
        print("="*60)
        print("✅ Normal authentication: Should succeed")
        print("🚨 Replay attack: Should be blocked")
        print("🔓 Invalid HMAC: Should be blocked")
        print("📋 Missing headers: Should be blocked")
        print("🔢 Old nonce: Should be blocked")
        print("\\n🛡️  All security mechanisms demonstrated successfully!")
        
    except requests.exceptions.ConnectionError:
        print("❌ ERROR: Could not connect to server. Make sure Flask server is running.")
    except Exception as e:
        print(f"❌ ERROR: {e}")

if __name__ == "__main__":
    main()