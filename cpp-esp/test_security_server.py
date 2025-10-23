"""
Simple Security Test Server for Milestone 4 Part 3
This server helps test the security implementation independently
"""

from flask import Flask, request, jsonify
from flask_cors import CORS
import hmac
import hashlib
import base64
import json
import time
from datetime import datetime

app = Flask(__name__)
CORS(app)

# Security Configuration - MUST MATCH device config.json
SECURITY_ENABLED = True
PSK = bytes.fromhex("c41716a134168f52fbd4be3302fa5a88127ddde749501a199607b4c286ad29b3")
NONCE_WINDOW = 100

# State Management
nonce_store = {}  # device_id -> last_seen_nonce
security_logs = []  # Log of all security events

def log_security_event(event_type, data):
    """Log security events for analysis"""
    event = {
        "timestamp": datetime.now().isoformat(),
        "type": event_type,
        "data": data
    }
    security_logs.append(event)
    print(f"[SECURITY] {event_type}: {json.dumps(data, indent=2)}")

def verify_hmac(payload_dict, received_mac):
    """Verify HMAC-SHA256 signature"""
    # Remove MAC from dict for verification
    payload_copy = payload_dict.copy()
    if "mac" in payload_copy:
        del payload_copy["mac"]
    
    # Sort keys for consistent ordering
    sorted_json = json.dumps(payload_copy, sort_keys=True)
    
    # Compute HMAC
    calculated_mac = hmac.new(PSK, sorted_json.encode(), hashlib.sha256).hexdigest()
    
    print(f"[DEBUG] Payload for HMAC: {sorted_json}")
    print(f"[DEBUG] Calculated MAC: {calculated_mac}")
    print(f"[DEBUG] Received MAC: {received_mac}")
    print(f"[DEBUG] Match: {calculated_mac == received_mac}")
    
    return calculated_mac == received_mac

def check_nonce(device_id, nonce):
    """Check if nonce is valid (not replayed)"""
    if device_id not in nonce_store:
        # First message from this device
        nonce_store[device_id] = nonce
        return True, "First nonce accepted"
    
    last_nonce = nonce_store[device_id]
    
    # Check if nonce is within acceptable window
    if nonce <= last_nonce:
        return False, f"Replay detected: nonce {nonce} <= last_nonce {last_nonce}"
    
    if nonce > last_nonce + NONCE_WINDOW:
        return False, f"Nonce too far ahead: nonce {nonce} > last_nonce {last_nonce} + {NONCE_WINDOW}"
    
    # Valid nonce
    nonce_store[device_id] = nonce
    return True, f"Nonce accepted: {nonce}"

@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint"""
    return jsonify({
        "status": "healthy",
        "service": "Security Test Server",
        "security_enabled": SECURITY_ENABLED,
        "timestamp": datetime.now().isoformat()
    }), 200

@app.route('/api/security/test', methods=['POST'])
def test_security():
    """
    Test endpoint for security verification
    Expects secured message format:
    {
        "nonce": 123,
        "timestamp": 1729012345678,
        "encrypted": false,
        "payload": "base64_encoded_json",
        "mac": "hmac_sha256_hex"
    }
    """
    try:
        data = request.get_json()
        device_id = request.headers.get('Device-ID', 'test-device')
        
        print(f"\n[REQUEST] Received from {device_id}")
        print(f"[REQUEST] Data: {json.dumps(data, indent=2)}")
        
        if not SECURITY_ENABLED:
            return jsonify({"status": "success", "message": "Security disabled"}), 200
        
        # Validate required fields
        required_fields = ["nonce", "timestamp", "payload", "mac"]
        missing = [f for f in required_fields if f not in data]
        if missing:
            log_security_event("VALIDATION_ERROR", {
                "device_id": device_id,
                "missing_fields": missing
            })
            return jsonify({"error": f"Missing fields: {missing}"}), 400
        
        # Extract fields
        nonce = data.get("nonce")
        timestamp = data.get("timestamp")
        encrypted = data.get("encrypted", False)
        payload_b64 = data.get("payload")
        received_mac = data.get("mac")
        
        # Step 1: Check nonce (anti-replay)
        nonce_valid, nonce_msg = check_nonce(device_id, nonce)
        if not nonce_valid:
            log_security_event("REPLAY_DETECTED", {
                "device_id": device_id,
                "nonce": nonce,
                "reason": nonce_msg
            })
            return jsonify({"error": "Replay attack detected", "details": nonce_msg}), 403
        
        print(f"[NONCE] {nonce_msg}")
        
        # Step 2: Verify HMAC
        hmac_valid = verify_hmac(data, received_mac)
        if not hmac_valid:
            log_security_event("HMAC_FAILED", {
                "device_id": device_id,
                "nonce": nonce
            })
            return jsonify({"error": "HMAC verification failed"}), 403
        
        print(f"[HMAC] Verification PASSED")
        
        # Step 3: Decode payload
        try:
            payload_json = base64.b64decode(payload_b64).decode('utf-8')
            payload_data = json.loads(payload_json)
            print(f"[PAYLOAD] Decoded: {json.dumps(payload_data, indent=2)}")
        except Exception as e:
            log_security_event("DECODE_ERROR", {
                "device_id": device_id,
                "error": str(e)
            })
            return jsonify({"error": f"Payload decode failed: {str(e)}"}), 400
        
        # Success!
        log_security_event("MESSAGE_VERIFIED", {
            "device_id": device_id,
            "nonce": nonce,
            "payload_type": payload_data.get("type", "unknown")
        })
        
        # Create secured response
        response_payload = {
            "status": "success",
            "message": "Security verification passed",
            "server_time": int(time.time() * 1000),
            "nonce_accepted": nonce
        }
        
        # Encode response
        response_json = json.dumps(response_payload, sort_keys=True)
        response_b64 = base64.b64encode(response_json.encode()).decode()
        
        # Create response envelope
        response_nonce = nonce + 1  # Server responds with next nonce
        response_envelope = {
            "nonce": response_nonce,
            "timestamp": int(time.time() * 1000),
            "encrypted": False,
            "payload": response_b64
        }
        
        # Compute response MAC
        response_mac = hmac.new(PSK, json.dumps(response_envelope, sort_keys=True).encode(), hashlib.sha256).hexdigest()
        response_envelope["mac"] = response_mac
        
        print(f"[RESPONSE] Sending secured response with nonce {response_nonce}")
        
        return jsonify(response_envelope), 200
        
    except Exception as e:
        print(f"[ERROR] {str(e)}")
        import traceback
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

@app.route('/api/security/logs', methods=['GET'])
def get_security_logs():
    """Get security event logs"""
    return jsonify({
        "logs": security_logs[-50:],  # Last 50 events
        "total_events": len(security_logs),
        "nonce_store": nonce_store
    }), 200

@app.route('/api/security/reset', methods=['POST'])
def reset_security():
    """Reset security state (for testing)"""
    global nonce_store, security_logs
    nonce_store = {}
    security_logs = []
    return jsonify({"status": "reset", "message": "Security state cleared"}), 200

if __name__ == '__main__':
    print("=" * 60)
    print("Security Test Server for Milestone 4 Part 3")
    print("=" * 60)
    print(f"Security Enabled: {SECURITY_ENABLED}")
    print(f"PSK: {PSK.hex()}")
    print(f"Nonce Window: {NONCE_WINDOW}")
    print("=" * 60)
    print("\nEndpoints:")
    print("  POST /api/security/test - Test secured messages")
    print("  GET /api/security/logs - View security logs")
    print("  POST /api/security/reset - Reset security state")
    print("  GET /health - Health check")
    print("\nListening on http://0.0.0.0:8080")
    print("=" * 60)
    
    app.run(host='0.0.0.0', port=8080, debug=True)
