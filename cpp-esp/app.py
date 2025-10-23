# EcoWatt Cloud API (Flask Example) — 15s inactivity debounce (per device)

from flask import Flask, request, jsonify
from flask_cors import CORS
import struct
import datetime
import threading
import time
import hmac
import hashlib
import base64
import json
import os
from pathlib import Path

app = Flask(__name__)
CORS(app)  # Enable CORS for all routes

# Log all incoming requests
@app.before_request
def log_request_info():
    if '/api/' in request.path:
        print(f"\n[REQUEST] {request.method} {request.path}")
        print(f"[REQUEST] Device-ID: {request.headers.get('Device-ID', 'None')}")
        print(f"[REQUEST] Authorization: {request.headers.get('Authorization', 'None')[:20]}...")

# Configuration persistence
CONFIG_FILE = Path("data/pending_configs.json")
HISTORY_FILE = Path("data/config_history.json")

# In-memory stores
UPLOADS = []
BENCHMARKS = []

# Simple register map for simulator
SIM_REGISTERS = {}
SIM_EXCEPTIONS = set()

# -------- Runtime Configuration Management --------
# Per-device pending config updates
PENDING_CONFIGS = {}  # device_id -> {nonce, config_update}
CONFIG_ACKS = []      # List of all config acknowledgments received
CONFIG_HISTORY = []   # Full history of config changes

def save_pending_configs():
    """Save pending configurations to disk."""
    try:
        CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)
        with open(CONFIG_FILE, 'w') as f:
            json.dump(PENDING_CONFIGS, f, indent=2)
        print(f"[CONFIG] Saved {len(PENDING_CONFIGS)} pending configs to disk")
    except Exception as e:
        print(f"[CONFIG] Error saving configs: {e}")

def load_pending_configs():
    """Load pending configurations from disk."""
    global PENDING_CONFIGS
    try:
        if CONFIG_FILE.exists():
            with open(CONFIG_FILE, 'r') as f:
                PENDING_CONFIGS = json.load(f)
            print(f"[CONFIG] Loaded {len(PENDING_CONFIGS)} pending configs from disk")
        else:
            print(f"[CONFIG] No saved configs found, starting fresh")
    except Exception as e:
        print(f"[CONFIG] Error loading configs: {e}")
        PENDING_CONFIGS = {}

def save_config_history():
    """Save configuration history to disk."""
    try:
        HISTORY_FILE.parent.mkdir(parents=True, exist_ok=True)
        with open(HISTORY_FILE, 'w') as f:
            json.dump(CONFIG_HISTORY[-100:], f, indent=2)  # Keep last 100 entries
    except Exception as e:
        print(f"[CONFIG] Error saving history: {e}")

def load_config_history():
    """Load configuration history from disk."""
    global CONFIG_HISTORY
    try:
        if HISTORY_FILE.exists():
            with open(HISTORY_FILE, 'r') as f:
                CONFIG_HISTORY = json.load(f)
            print(f"[CONFIG] Loaded {len(CONFIG_HISTORY)} history entries from disk")
    except Exception as e:
        print(f"[CONFIG] Error loading history: {e}")
        CONFIG_HISTORY = []

# -------- Command Execution Management --------
PENDING_COMMANDS = {}  # device_id -> {nonce, command}
COMMAND_RESULTS = []   # List of all command execution results
COMMAND_HISTORY = []   # Full history of commands

# -------- FOTA Management --------
FIRMWARE_MANIFEST = None
FIRMWARE_CHUNKS = {}   # chunk_number -> {data, mac}
FOTA_STATUS = {}       # device_id -> {chunk_received, verified, last_update}

# -------- Security --------
# MILESTONE 4 PART 3: Security Implementation
SECURITY_ENABLED = True  # Security enabled with proper envelope wrapping
PRE_SHARED_KEY = bytes.fromhex("c41716a134168f52fbd4be3302fa5a88127ddde749501a199607b4c286ad29b3")
PSK = bytes.fromhex("c41716a134168f52fbd4be3302fa5a88127ddde749501a199607b4c286ad29b3")  # 256-bit PSK
NONCE_STORE = {}  # device_id -> {'nonce': last_nonce, 'timestamp': last_seen_time}
NONCE_WINDOW = 100  # Allow nonces within this window
NONCE_EXPIRY_SECONDS = 75  # Clear nonces older than 75 seconds (allows device reboots during testing)
SERVER_NONCE_COUNTER = 300  # Server nonce counter - start ahead of device's current ~204

def create_secured_response(payload_dict, device_id=None):
    """
    Wrap response in secured envelope format.
    Returns: {nonce, timestamp, encrypted, payload, mac}
    """
    global SERVER_NONCE_COUNTER
    
    # If we have device context, sync with device's nonce range
    if device_id:
        nonce_data = NONCE_STORE.get(device_id)
        if nonce_data:
            device_last_nonce = nonce_data['nonce']
            # Keep response nonce within device's 100-nonce window
            # For first-time devices (last_received_nonce=0), start very low
            if device_last_nonce < 50:  # First-time communication
                # For brand new devices, use nonce close to device's outgoing nonce
                SERVER_NONCE_COUNTER = max(1, min(50, device_last_nonce + 1))
            else:
                # Use a nonce just ahead of device's last nonce (within window)
                SERVER_NONCE_COUNTER = max(SERVER_NONCE_COUNTER, device_last_nonce + 1)
        else:
            # No previous communication, start with very low nonce for first contact
            SERVER_NONCE_COUNTER = 1
    else:
        # No device context, ensure we don't go too high
        if SERVER_NONCE_COUNTER > 50:
            SERVER_NONCE_COUNTER = 1
    
    SERVER_NONCE_COUNTER += 1
    nonce = SERVER_NONCE_COUNTER
    timestamp = int(time.time())
    encrypted = True  # Enable server-side encryption for demonstration
    
    # Convert payload to JSON string
    payload_str = json.dumps(payload_dict)
    
    # If encryption enabled, encode payload as base64 (simulated encryption)
    if encrypted:
        payload_str = base64.b64encode(payload_str.encode()).decode()
    
    # Compute HMAC: nonce + timestamp + encrypted_flag + payload
    hmac_input = f"{nonce}{timestamp}{'1' if encrypted else '0'}{payload_str}"
    mac = hmac.new(PSK, hmac_input.encode(), hashlib.sha256).hexdigest()
    
    return {
        'nonce': nonce,
        'timestamp': timestamp,
        'encrypted': encrypted,
        'payload': payload_str,
        'mac': mac
    }

def verify_request_security(device_id):
    """
    Verify security headers (X-Nonce, X-Timestamp, X-MAC) for GET requests.
    Returns (success: bool, error_message: str)
    """
    if not SECURITY_ENABLED:
        return True, None
    
    # Get security headers
    nonce = request.headers.get('X-Nonce')
    timestamp = request.headers.get('X-Timestamp')
    received_mac = request.headers.get('X-MAC')
    
    if not nonce or not timestamp or not received_mac:
        msg = f"Missing security headers: nonce={nonce}, ts={timestamp}, mac={received_mac}"
        log_security_event(device_id, 'missing_headers', msg)
        return False, msg
    
    try:
        nonce_int = int(nonce)
    except ValueError:
        msg = f"Invalid nonce format: {nonce}"
        log_security_event(device_id, 'invalid_nonce', msg)
        return False, msg
    
    # Check nonce for replay protection with expiry handling
    current_time = time.time()
    nonce_data = NONCE_STORE.get(device_id)
    
    if nonce_data:
        last_nonce = nonce_data['nonce']
        last_seen = nonce_data['timestamp']
        
        # If nonce is old (device likely rebooted), clear it
        if (current_time - last_seen) > NONCE_EXPIRY_SECONDS:
            print(f"[SECURITY] Clearing expired nonce for {device_id} (age: {current_time - last_seen:.0f}s)")
            NONCE_STORE.pop(device_id, None)
        elif nonce_int <= last_nonce:
            msg = f"Replay attack detected: nonce {nonce_int} <= {last_nonce}"
            log_security_event(device_id, 'replay_attack', msg)
            return False, msg
    
    # Verify HMAC: endpoint + nonce + timestamp
    hmac_input = request.path + nonce + timestamp
    calculated_mac = hmac.new(PSK, hmac_input.encode(), hashlib.sha256).hexdigest()
    
    if not hmac.compare_digest(calculated_mac, received_mac):
        msg = f"HMAC verification failed"
        log_security_event(device_id, 'hmac_failed', msg)
        return False, msg
    
    # Update nonce with timestamp
    NONCE_STORE[device_id] = {'nonce': nonce_int, 'timestamp': current_time}
    log_security_event(device_id, 'hmac_verified', f"Nonce: {nonce_int}")
    return True, None

# -------- Logging --------
SECURITY_LOGS = []  # Security events (HMAC failures, replay attacks, etc.)
FOTA_LOGS = []      # FOTA operations (upload, download, verify, rollback)
COMMAND_LOGS = []   # Command forwarding to Modbus (detailed)

# -------- Debounced batching state (per device) --------
FLUSH_INTERVAL_SEC = 15

# BUFFERS[device_id] = {
#   "reg_values": { reg_addr: [values...] },
#   "received_bytes": int,
#   "last_seen": float (epoch seconds)  # when we last received data for this device
# }
BUFFERS = {}
BUFF_LOCK = threading.Lock()

def _now_epoch() -> float:
    return time.time()

def _flush_device_unlocked(device_id: str):
    """
    Compute per-register averages for a device buffer and push one record.
    CALL ONLY WITH BUFF_LOCK HELD.
    """
    buf = BUFFERS.get(device_id)
    if not buf:
        return None

    reg_values = buf.get("reg_values", {})
    received_bytes = int(buf.get("received_bytes", 0))

    # Nothing to flush?
    has_any = any(len(vals) for vals in reg_values.values())
    if not has_any:
        # keep buffer but reset counters
        BUFFERS[device_id] = {
            "reg_values": {},
            "received_bytes": 0,
            "last_seen": buf.get("last_seen", _now_epoch()),
        }
        return None

    # Build averaged samples (one per register with values)
    flush_dt = datetime.datetime.now().isoformat()
    averaged_samples = []
    values_flat = []
    for reg, vals in sorted(reg_values.items()):
        if not vals:
            continue
        avg_val = sum(vals) / len(vals)
        values_flat.extend(vals)
        averaged_samples.append({
            "timestamp": int(_now_epoch()),    # flush time as sample ts
            "reg_addr": reg,
            "value": round(float(avg_val), 3)
        })

    # Save upload record (this is what Node-RED reads)
    upload_record = {
        "timestamp": flush_dt,
        "device_id": device_id,
        "bytes": received_bytes,
        "samples": averaged_samples
    }
    UPLOADS.append(upload_record)

    # Prepare benchmark for Node-RED push
    num_samples = len(averaged_samples)
    original_size = num_samples * 4 * 3  # keep your earlier convention
    compressed_size = received_bytes
    compression_ratio = (round(original_size / compressed_size, 2)
                         if compressed_size > 0 else "N/A")
    min_v = min(values_flat) if values_flat else None
    max_v = max(values_flat) if values_flat else None
    avg_v = (round(sum(values_flat) / len(values_flat), 2)
             if values_flat else None)

    benchmark = {
        "method": "delta-avg-15s-inactivity",
        "num_samples": num_samples,
        "original_size": original_size,
        "compressed_size": compressed_size,
        "compression_ratio": compression_ratio,
        "lossless_verified": False,  # aggregate, not raw
        "cpu_time_ms": None,
        "min": min_v,
        "avg": avg_v,
        "max": max_v
    }

    flask_push_payload = {
        "device_id": device_id,
        "timestamp": flush_dt,
        "benchmark": benchmark,
        "samples": [s["value"] for s in averaged_samples]
    }

    # Reset buffer AFTER we've computed everything
    BUFFERS[device_id] = {
        "reg_values": {},
        "received_bytes": 0,
        "last_seen": _now_epoch(),  # sets a new baseline of inactivity
    }

    # Push to Node-RED (same endpoint/body as before)
    try:
        import requests
        node_red_url = "http://localhost:1880/api/flask_push"
        resp = requests.post(node_red_url, json=flask_push_payload, timeout=2)
        print(f"[DEBUG] flush->Node-RED: device={device_id}, status={resp.status_code}")
    except Exception as e:
        print(f"[ERROR] flush->Node-RED failed: {e}")

    return upload_record

def _flusher_loop():
    """Flush a device only after 15s of INACTIVITY since its last upload."""
    print(f"[INFO] Flusher thread started (debounce interval={FLUSH_INTERVAL_SEC}s)")
    while True:
        time.sleep(1)
        try:
            now = _now_epoch()
            with BUFF_LOCK:
                # copy keys to avoid mutation during iteration
                items = list(BUFFERS.items())
            for device_id, buf in items:
                last_seen = buf.get("last_seen", 0.0)
                has_data = any(len(v) for v in buf.get("reg_values", {}).values())
                if has_data and (now - last_seen) >= FLUSH_INTERVAL_SEC:
                    with BUFF_LOCK:
                        _flush_device_unlocked(device_id)
        except Exception as e:
            print(f"[ERROR] flusher loop: {e}")

# ---------------- Existing endpoints (unchanged signatures) ----------------

@app.route('/api/upload/meta', methods=['POST'])
def upload_meta():
    try:
        meta = request.get_json(force=True)
        print(f"[BENCHMARK] Compression Method: {meta.get('compression_method')}")
        print(f"[BENCHMARK] Number of Samples: {meta.get('num_samples')}")
        print(f"[BENCHMARK] Original Payload Size: {meta.get('original_size')}")
        print(f"[BENCHMARK] Compressed Payload Size: {meta.get('compressed_size')}")
        print(f"[BENCHMARK] Compression Ratio: {meta.get('compression_ratio')}")
        print(f"[BENCHMARK] CPU Time (ms): {meta.get('cpu_time_ms')}")
        print(f"[BENCHMARK] Lossless Recovery Verification: {meta.get('lossless')}")
        print(f"[BENCHMARK] Aggregation min/avg/max: {meta.get('min')}, {meta.get('avg')}, {meta.get('max')}")
        BENCHMARKS.append(meta)
        return jsonify({'status': 'success', 'benchmark': meta})
    except Exception as e:
        print(f"[ERROR] /api/upload/meta: {e}")
        return jsonify({'status': 'error', 'error': str(e)}), 400

def decompress_delta_payload(data: bytes):
    """
    Decompresses the binary payload from the device.
    Format for each sample:
      [timestamp (4 bytes, uint32 LE)]
      [reg_addr (1 byte)]
      [value (4 bytes, float32 LE)]
    """
    print(f"[DEBUG] decompress_delta_payload: received {len(data)} bytes")
    sample_size = 4 + 1 + 4
    if len(data) < sample_size:
        print("[DEBUG] decompress_delta_payload: data too short")
        return []

    samples = []
    offset = 0
    idx = 0
    while offset + sample_size <= len(data):
        ts, reg_addr, value = struct.unpack('<IBf', data[offset:offset+sample_size])
        print(f"[DEBUG] decompress_delta_payload: sample {idx}: ts={ts}, reg_addr={reg_addr}, value={value}")
        samples.append({'timestamp': int(ts), 'reg_addr': reg_addr, 'value': round(float(value), 3)})
        offset += sample_size
        idx += 1
    print(f"[DEBUG] decompress_delta_payload: total samples={len(samples)}")
    return samples

@app.route('/api/upload', methods=['POST'])
def upload():
    device_id = request.headers.get('device-id') or request.headers.get('Device-ID') or 'Unknown-Device'
    
    # Check if this is a secured request (JSON envelope)
    content_type = request.headers.get('Content-Type', '')
    if 'application/json' in content_type and SECURITY_ENABLED:
        try:
            # Parse secured envelope
            envelope = request.get_json(force=True)
            if 'payload' in envelope and 'nonce' in envelope and 'mac' in envelope:
                print(f"[DEBUG] /api/upload: Received secured envelope from {device_id}")
                
                # Verify MAC
                payload_str = envelope['payload']
                received_mac = envelope['mac']
                nonce = envelope['nonce']
                timestamp = envelope.get('timestamp', 0)
                encrypted_flag = envelope.get('encrypted', False)
                
                # Verify HMAC (must match ESP32: nonce + timestamp + encrypted + payload)
                hmac_input = f"{nonce}{timestamp}{'1' if encrypted_flag else '0'}{payload_str}"
                calculated_mac = hmac.new(PSK, hmac_input.encode(), hashlib.sha256).hexdigest()
                
                if not hmac.compare_digest(calculated_mac, received_mac):
                    log_security_event(device_id, 'hmac_failed', f"Upload HMAC mismatch")
                    return jsonify({'error': 'Unauthorized', 'details': 'HMAC verification failed'}), 401
                
                # Check nonce
                last_nonce = NONCE_STORE.get(device_id, 0)
                if nonce <= last_nonce:
                    log_security_event(device_id, 'replay_attack', f'Upload nonce {nonce} <= {last_nonce}')
                    return jsonify({'error': 'Unauthorized', 'details': 'Replay attack detected'}), 401
                
                NONCE_STORE[device_id] = nonce
                log_security_event(device_id, 'hmac_verified', f"Upload authenticated, nonce: {nonce}")
                
                # Decode payload (base64-encoded binary data)
                try:
                    compressed_payload = base64.b64decode(payload_str)
                except Exception as e:
                    return jsonify({'error': 'Invalid payload encoding'}), 400
            else:
                compressed_payload = request.data
        except Exception as e:
            print(f"[ERROR] /api/upload: Failed to parse secured envelope: {e}")
            compressed_payload = request.data
    else:
        # Plain binary upload (legacy)
        compressed_payload = request.data
    
    print(f"[DEBUG] /api/upload called: device_id={device_id}, payload_bytes={len(compressed_payload)}")

    try:
        samples = decompress_delta_payload(compressed_payload)
        print(f"[DEBUG] /api/upload: decompressed {len(samples)} samples")
    except Exception as e:
        print(f"[ERROR] /api/upload: decompression failed: {e}")
        samples = []

    # Accumulate into debounce buffer (NO immediate push)
    now = _now_epoch()
    with BUFF_LOCK:
        buf = BUFFERS.get(device_id)
        if not buf:
            buf = {"reg_values": {}, "received_bytes": 0, "last_seen": now}
            BUFFERS[device_id] = buf

        for s in samples:
            reg = s.get("reg_addr")
            val = s.get("value")
            if reg is None or val is None:
                continue
            buf["reg_values"].setdefault(reg, []).append(float(val))

        buf["received_bytes"] += int(len(compressed_payload))
        buf["last_seen"] = now  # debounce: reset the inactivity timer on every upload

    # Immediate ACK; flush occurs after 15s of inactivity
    return jsonify({'status': 'success', 'received': len(compressed_payload)})

@app.route('/api/uploads', methods=['GET'])
def get_uploads():
    # Expose flushed records and benchmark meta
    return jsonify({'uploads': UPLOADS, 'benchmarks': BENCHMARKS})

# ----------------- Modbus simulator (unchanged) -----------------

def compute_crc(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF

@app.route('/api/inverter/read', methods=['POST'])
def inverter_read():
    # Expecting JSON: {"frame": "<HEX>"}
    j = request.get_json(silent=True)
    if not j or 'frame' not in j:
        return jsonify({'error': 'no frame provided'}), 400
    hex_frame = j['frame']
    try:
        data = bytes.fromhex(hex_frame)
    except Exception:
        return jsonify({'error': 'invalid hex'}), 400

    if len(data) < 8:
        return jsonify({'error': 'frame too short'}), 400
    slave = data[0]
    func = data[1]

    if func == 0x03:
        num_regs = (data[4] << 8) | data[5]
        start_addr = (data[2] << 8) | data[3]
        if start_addr in SIM_EXCEPTIONS:
            payload = bytearray()
            payload.append(slave)
            payload.append(0x83)
            payload.append(0x02)
            crc = compute_crc(payload)
            payload.append(crc & 0xFF)
            payload.append((crc >> 8) & 0xFF)
            return jsonify({'frame': payload.hex().upper()})
        byte_count = num_regs * 2
        payload = bytearray()
        payload.append(slave)
        payload.append(0x03)
        payload.append(byte_count)
        for i in range(num_regs):
            addr = start_addr + i
            if addr in SIM_REGISTERS:
                val = SIM_REGISTERS[addr]
            else:
                if addr == 0:
                    val = 2300
                elif addr == 1:
                    val = 25
                elif addr == 2:
                    val = 5000
                elif addr == 3:
                    val = 3200
                elif addr == 4:
                    val = 3150
                elif addr == 5:
                    val = 85
                elif addr == 6:
                    val = 82
                elif addr == 7:
                    val = 350
                elif addr == 8:
                    val = 75
                elif addr == 9:
                    val = 1850
                else:
                    val = (i + 1) * 10
            payload.append((val >> 8) & 0xFF)
            payload.append(val & 0xFF)
        crc = compute_crc(payload)
        payload.append(crc & 0xFF)
        payload.append((crc >> 8) & 0xFF)
        return jsonify({'frame': payload.hex().upper()})
    else:
        return jsonify({'error': 'unsupported function in sim'}), 400

@app.route('/api/inverter/write', methods=['POST'])
def inverter_write():
    j = request.get_json(silent=True)
    if not j or 'frame' not in j:
        return jsonify({'error': 'no frame provided'}), 400
    hex_frame = j['frame']
    try:
        data = bytes.fromhex(hex_frame)
    except Exception:
        return jsonify({'error': 'invalid hex'}), 400

    if len(data) >= 6:
        core = data[:-2]
        crc = compute_crc(core)
        resp = bytearray(core)
        resp.append(crc & 0xFF)
        resp.append((crc >> 8) & 0xFF)
        return jsonify({'frame': resp.hex().upper()})
    return jsonify({'error': 'frame too short'}), 400

@app.route('/api/inverter/config', methods=['POST'])
def inverter_config():
    j = request.get_json(silent=True)
    if not j:
        return jsonify({'error': 'invalid json'}), 400
    regs = j.get('registers')
    if regs:
        for k, v in regs.items():
            try:
                addr = int(k)
                SIM_REGISTERS[addr] = int(v)
            except Exception:
                pass
    ex = j.get('exceptions')
    if ex is not None:
        SIM_EXCEPTIONS.clear()
        for a in ex:
            try:
                SIM_EXCEPTIONS.add(int(a))
            except Exception:
                pass
    return jsonify({'status': 'ok', 'registers': SIM_REGISTERS, 'exceptions': list(SIM_EXCEPTIONS)})

# ============ RUNTIME CONFIGURATION ENDPOINTS ============

@app.route('/api/inverter/config/simple', methods=['GET'])
def get_device_config_simple():
    """
    SIMPLIFIED configuration endpoint WITHOUT security checks.
    Device polls this endpoint for pending configuration updates.
    Returns pending config if available, empty otherwise.
    Use this for testing/debugging configuration flow.
    """
    device_id = request.headers.get('Device-ID') or request.args.get('device_id') or 'EcoWatt001'
    
    print(f"[CONFIG-SIMPLE] ====== SIMPLE ENDPOINT CALLED ======")
    print(f"[CONFIG-SIMPLE] Request from device: {device_id}")
    print(f"[CONFIG-SIMPLE] Request path: {request.path}")
    print(f"[CONFIG-SIMPLE] Request method: {request.method}")
    
    pending = PENDING_CONFIGS.get(device_id)
    if pending:
        print(f"[CONFIG-SIMPLE] Sending pending config to {device_id}: {pending}")
        return jsonify(pending)
    
    # No pending config
    print(f"[CONFIG-SIMPLE] No pending config for {device_id}")
    return jsonify({
        "status": "no_config",
        "message": "No pending configuration updates"
    })

@app.route('/api/inverter/config', methods=['GET'])
def get_device_config():
    """
    Device polls this endpoint for pending configuration updates.
    Returns pending config if available, empty otherwise.
    """
    device_id = request.headers.get('Device-ID') or request.args.get('device_id') or 'EcoWatt001'
    
    print(f"[DEBUG] /api/inverter/config called by device: {device_id}")
    print(f"[DEBUG] Headers: X-Nonce={request.headers.get('X-Nonce')}, X-Timestamp={request.headers.get('X-Timestamp')}, X-MAC={request.headers.get('X-MAC')}")
    
    # Verify security
    success, error_msg = verify_request_security(device_id)
    if not success:
        print(f"[CONFIG] Security verification failed for {device_id}: {error_msg}")
        return jsonify({'error': 'Unauthorized', 'details': error_msg}), 401
    
    pending = PENDING_CONFIGS.get(device_id)
    if pending:
        print(f"[CONFIG] Sending pending config to {device_id}: {pending}")
        
        # Wrap in secured envelope if security enabled
        if SECURITY_ENABLED:
            response = create_secured_response(pending, device_id)
            print(f"[DEBUG] Secured response: {response}")
            return jsonify(response)
        else:
            return jsonify(pending)
    
    # No pending config - return empty response (also wrapped if security enabled)
    print(f"[CONFIG] No pending config for {device_id}")
    if SECURITY_ENABLED:
        # Send a proper "no config" structure that the device can parse
        global SERVER_NONCE_COUNTER
        SERVER_NONCE_COUNTER += 1
        empty_payload = {
            "status": "no_config",
            "nonce": SERVER_NONCE_COUNTER,
            "message": "No pending configuration updates"
        }
        response = create_secured_response(empty_payload, device_id)
        print(f"[DEBUG] Empty secured response: {response}")
        return jsonify(response)
    else:
        return jsonify({"status": "no_config", "message": "No pending configuration updates"})

@app.route('/api/inverter/config/ack', methods=['POST'])
def receive_config_ack():
    """
    Device sends acknowledgment after processing config update.
    Format: {nonce, timestamp, all_success, config_ack: {accepted, rejected, unchanged}}
    """
    # Check if this is a secured request
    content_type = request.headers.get('Content-Type', '')
    if 'application/json' in content_type and SECURITY_ENABLED:
        try:
            envelope = request.get_json(force=True)
            if 'payload' in envelope and 'nonce' in envelope and 'mac' in envelope:
                # Extract and verify
                payload_str = envelope['payload']
                received_mac = envelope['mac']
                nonce = envelope['nonce']
                timestamp = envelope.get('timestamp', 0)
                encrypted_flag = envelope.get('encrypted', False)
                
                # Verify HMAC
                hmac_input = f"{nonce}{timestamp}{'1' if encrypted_flag else '0'}{payload_str}"
                calculated_mac = hmac.new(PSK, hmac_input.encode(), hashlib.sha256).hexdigest()
                
                if not hmac.compare_digest(calculated_mac, received_mac):
                    return jsonify({'error': 'Unauthorized', 'details': 'HMAC verification failed'}), 401
                
                # Parse payload
                ack = json.loads(payload_str)
            else:
                ack = request.get_json(force=True)
        except Exception as e:
            print(f"[ERROR] Failed to parse secured ack: {e}")
            ack = request.get_json(force=True)
    else:
        ack = request.get_json(force=True)
    
    device_id = request.headers.get('Device-ID') or 'EcoWatt001'
    
    # Store acknowledgment
    ack['device_id'] = device_id
    ack['received_at'] = datetime.datetime.now().isoformat()
    CONFIG_ACKS.append(ack)
    
    # Log to history
    history_entry = {
        'device_id': device_id,
        'timestamp': ack['received_at'],
        'nonce': ack.get('nonce'),
        'all_success': ack.get('all_success'),
        'accepted': ack.get('config_ack', {}).get('accepted', []),
        'rejected': ack.get('config_ack', {}).get('rejected', []),
        'unchanged': ack.get('config_ack', {}).get('unchanged', [])
    }
    CONFIG_HISTORY.append(history_entry)
    
    # Clear pending config if nonce matches
    pending = PENDING_CONFIGS.get(device_id)
    if pending and pending.get('nonce') == ack.get('nonce'):
        PENDING_CONFIGS.pop(device_id)
    
    print(f"[CONFIG ACK] Received from {device_id}: all_success={ack.get('all_success')}")
    print(f"[CONFIG ACK] Accepted: {len(history_entry['accepted'])}, Rejected: {len(history_entry['rejected'])}, Unchanged: {len(history_entry['unchanged'])}")
    
    # Return secured response
    response_data = {'status': 'success', 'message': 'Acknowledgment received'}
    if SECURITY_ENABLED:
        return jsonify(create_secured_response(response_data))
    else:
        return jsonify(response_data)

@app.route('/api/cloud/config/send', methods=['POST'])
def send_config_update():
    """
    Cloud admin endpoint to send configuration update to device.
    Request: {device_id, sampling_interval (seconds), registers: [list]}
    """
    req = request.get_json(force=True)
    device_id = req.get('device_id', 'EcoWatt001')
    sampling_interval = req.get('sampling_interval')
    registers = req.get('registers', [])
    
    # Generate nonce - use device-compatible range
    global SERVER_NONCE_COUNTER
    SERVER_NONCE_COUNTER += 1
    nonce = SERVER_NONCE_COUNTER
    
    # Build config update message
    config_update = {}
    if sampling_interval is not None:
        config_update['sampling_interval'] = int(sampling_interval)
    if registers:
        config_update['registers'] = registers
    
    pending_config = {
        'nonce': nonce,
        'config_update': config_update
    }
    
    # Store pending config for device
    PENDING_CONFIGS[device_id] = pending_config
    
    # Save to disk immediately
    save_pending_configs()
    
    # Add to history
    history_entry = {
        'timestamp': datetime.datetime.now().isoformat(),
        'device_id': device_id,
        'nonce': nonce,
        'config': config_update,
        'status': 'queued'
    }
    CONFIG_HISTORY.append(history_entry)
    save_config_history()
    
    print(f"[CONFIG] Queued config update for {device_id}: nonce={nonce}, interval={sampling_interval}s, registers={registers}")
    
    return jsonify({
        'status': 'success',
        'message': f'Configuration update queued for {device_id}',
        'nonce': nonce,
        'config': config_update
    })

@app.route('/api/cloud/config/history', methods=['GET'])
def get_config_history():
    """
    Get configuration update history for dashboard/monitoring.
    """
    device_id = request.args.get('device_id')
    
    history = CONFIG_HISTORY
    if device_id:
        history = [h for h in CONFIG_HISTORY if h.get('device_id') == device_id]
    
    return jsonify({
        'total': len(history),
        'history': history[-50:]  # Last 50 entries
    })

# ============ COMMAND EXECUTION ENDPOINTS ============

@app.route('/api/cloud/command/send', methods=['POST'])
def send_command():
    """
    Cloud admin endpoint to send command to device.
    Request: {device_id, action, target_register, value, encrypted (optional)}
    """
    req = request.get_json(force=True)
    device_id = req.get('device_id', 'EcoWatt001')
    action = req.get('action', 'write_register')
    target_register = req.get('target_register')
    value = req.get('value')
    use_encryption = req.get('encrypted', False)
    
    # Generate nonce
    nonce = int(time.time() * 1000)
    
    # Build command message
    command_data = {
        'action': action,
        'target_register': target_register,
        'value': value
    }
    
    # Encrypt payload if requested
    command_json = json.dumps(command_data)
    if use_encryption:
        encrypted_payload = encrypt_payload(command_json)
        command = {
            'nonce': nonce,
            'encrypted': True,
            'payload': encrypted_payload
        }
        log_security_event(device_id, 'command_encrypted', f'Command encrypted for nonce {nonce}')
    else:
        command = {
            'nonce': nonce,
            'command': command_data
        }
    
    # Add HMAC for security
    mac = hmac.new(PRE_SHARED_KEY, command_json.encode(), hashlib.sha256).hexdigest()
    command['mac'] = mac
    
    # Store pending command
    PENDING_COMMANDS[device_id] = command
    
    # Log to history
    COMMAND_HISTORY.append({
        'device_id': device_id,
        'timestamp': datetime.datetime.now().isoformat(),
        'nonce': nonce,
        'action': action,
        'target_register': target_register,
        'value': value,
        'status': 'pending',
        'encrypted': use_encryption
    })
    
    # Log command event
    log_command_event(device_id, 'command_queued', 
                     f'Action: {action}, Register: {target_register}, Value: {value}, Encrypted: {use_encryption}')
    
    print(f"[COMMAND] Queued command for {device_id}: {action} on {target_register} = {value} (encrypted={use_encryption})")
    
    return jsonify({
        'status': 'success',
        'message': f'Command queued for {device_id}',
        'nonce': nonce,
        'encrypted': use_encryption
    })

@app.route('/api/inverter/command', methods=['GET'])
def get_pending_command():
    """
    Device polls this endpoint for pending commands.
    """
    device_id = request.headers.get('Device-ID') or request.args.get('device_id') or 'EcoWatt001'
    
    pending = PENDING_COMMANDS.get(device_id)
    if pending:
        print(f"[COMMAND] Sending pending command to {device_id}")
        return jsonify(pending)
    
    return jsonify({})

@app.route('/api/inverter/command/result', methods=['POST'])
def receive_command_result():
    """
    Device sends command execution result.
    Format: {nonce, command_result: {status, executed_at, modbus_response (optional)}}
    """
    result = request.get_json(force=True)
    device_id = request.headers.get('Device-ID') or 'EcoWatt001'
    
    # Verify nonce (anti-replay)
    nonce = result.get('nonce')
    if nonce and not check_nonce(device_id, nonce):
        log_security_event(device_id, 'replay_attack_command_result', f'Nonce: {nonce}')
        return jsonify({'status': 'error', 'message': 'Invalid nonce (replay attack)'}), 403
    
    # Store result
    result['device_id'] = device_id
    result['received_at'] = datetime.datetime.now().isoformat()
    COMMAND_RESULTS.append(result)
    
    # Extract command result details
    cmd_result = result.get('command_result', {})
    status = cmd_result.get('status', 'unknown')
    executed_at = cmd_result.get('executed_at')
    modbus_response = cmd_result.get('modbus_response', 'N/A')
    modbus_frame = cmd_result.get('modbus_frame', 'N/A')
    
    # Update history
    for entry in COMMAND_HISTORY:
        if entry.get('device_id') == device_id and entry.get('nonce') == nonce:
            entry['status'] = status
            entry['executed_at'] = executed_at
            entry['modbus_response'] = modbus_response
            break
    
    # Log command execution details
    log_command_event(device_id, f'command_result_{status}', 
                     f'Nonce: {nonce}, Executed: {executed_at}, Modbus Response: {modbus_response}')
    
    # If Modbus frame is provided, log it
    if modbus_frame != 'N/A':
        log_command_event(device_id, 'modbus_frame_sent', f'Frame: {modbus_frame}')
    
    # Clear pending command if nonce matches
    pending = PENDING_COMMANDS.get(device_id)
    if pending and pending.get('nonce') == nonce:
        PENDING_COMMANDS.pop(device_id)
        log_command_event(device_id, 'command_completed', f'Nonce: {nonce}, Status: {status}')
    
    print(f"[COMMAND RESULT] Received from {device_id}: status={status}, modbus={modbus_response}")
    
    return jsonify({'status': 'success', 'message': 'Command result received'})

@app.route('/api/cloud/command/history', methods=['GET'])
def get_command_history():
    """
    Get command execution history.
    """
    device_id = request.args.get('device_id')
    
    history = COMMAND_HISTORY
    if device_id:
        history = [h for h in COMMAND_HISTORY if h.get('device_id') == device_id]
    
    return jsonify({
        'total': len(history),
        'history': history[-50:]
    })

# ============ FOTA ENDPOINTS ============

@app.route('/api/cloud/fota/upload', methods=['POST'])
def upload_firmware():
    """
    Cloud admin endpoint to upload firmware for OTA.
    Request: {version, size, hash, chunk_size, firmware_data (base64)}
    """
    req = request.get_json(force=True)
    
    version = req.get('version')
    size = req.get('size')
    fw_hash = req.get('hash')
    chunk_size = req.get('chunk_size', 1024)
    firmware_data_b64 = req.get('firmware_data')
    
    if not all([version, size, fw_hash, firmware_data_b64]):
        log_fota_event('cloud', 'upload_failed', 'Missing required fields')
        return jsonify({'error': 'Missing required fields'}), 400
    
    # Decode firmware data
    try:
        firmware_data = base64.b64decode(firmware_data_b64)
        log_fota_event('cloud', 'firmware_decoded', f'Size: {len(firmware_data)} bytes')
    except Exception as e:
        log_fota_event('cloud', 'upload_failed', f'Invalid base64: {e}')
        return jsonify({'error': f'Invalid base64: {e}'}), 400
    
    # Verify hash
    calculated_hash = hashlib.sha256(firmware_data).hexdigest()
    if calculated_hash != fw_hash:
        log_fota_event('cloud', 'upload_failed', f'Hash mismatch: expected {fw_hash}, got {calculated_hash}')
        return jsonify({'error': 'Hash mismatch'}), 400
    
    log_fota_event('cloud', 'firmware_hash_verified', f'Hash: {fw_hash}')
    
    # Create manifest
    global FIRMWARE_MANIFEST
    FIRMWARE_MANIFEST = {
        'version': version,
        'size': size,
        'hash': fw_hash,
        'chunk_size': chunk_size,
        'uploaded_at': datetime.datetime.now().isoformat()
    }
    
    # Split into chunks
    FIRMWARE_CHUNKS.clear()
    num_chunks = (len(firmware_data) + chunk_size - 1) // chunk_size
    
    log_fota_event('cloud', 'chunking_started', f'Creating {num_chunks} chunks of {chunk_size} bytes')
    
    for i in range(num_chunks):
        start = i * chunk_size
        end = min(start + chunk_size, len(firmware_data))
        chunk_data = firmware_data[start:end]
        chunk_b64 = base64.b64encode(chunk_data).decode('ascii')
        
        # Generate HMAC for chunk
        mac = hmac.new(PRE_SHARED_KEY, chunk_data, hashlib.sha256).hexdigest()
        
        FIRMWARE_CHUNKS[i] = {
            'chunk_number': i,
            'data': chunk_b64,
            'mac': mac,
            'size': len(chunk_data)
        }
    
    log_fota_event('cloud', 'firmware_uploaded', 
                  f'Version: {version}, Size: {size} bytes, Chunks: {num_chunks}, Hash: {fw_hash}')
    
    print(f"[FOTA] Firmware uploaded: version={version}, size={size}, chunks={num_chunks}")
    
    return jsonify({
        'status': 'success',
        'message': 'Firmware uploaded and chunked',
        'manifest': FIRMWARE_MANIFEST,
        'total_chunks': num_chunks
    })

@app.route('/api/inverter/fota/manifest', methods=['GET'])
def get_fota_manifest():
    """
    Device polls for FOTA manifest.
    """
    if FIRMWARE_MANIFEST:
        return jsonify({'fota': {'manifest': FIRMWARE_MANIFEST}})
    return jsonify({})

@app.route('/api/inverter/fota/chunk', methods=['GET'])
def get_fota_chunk():
    """
    Device requests specific firmware chunk.
    Query: chunk_number
    """
    chunk_num = int(request.args.get('chunk_number', 0))
    
    chunk = FIRMWARE_CHUNKS.get(chunk_num)
    if chunk:
        return jsonify(chunk)
    
    return jsonify({'error': 'Chunk not found'}), 404

@app.route('/api/inverter/fota/status', methods=['POST'])
def receive_fota_status():
    """
    Device sends FOTA status/acknowledgment.
    Format: {fota_status: {chunk_received, verified, boot_status, rollback, error}}
    """
    status = request.get_json(force=True)
    device_id = request.headers.get('Device-ID') or 'EcoWatt001'
    
    fota_status = status.get('fota_status', {})
    chunk_received = fota_status.get('chunk_received')
    verified = fota_status.get('verified')
    boot_status = fota_status.get('boot_status')
    rollback = fota_status.get('rollback', False)
    error = fota_status.get('error')
    
    # Update device FOTA status
    FOTA_STATUS[device_id] = {
        'chunk_received': chunk_received,
        'verified': verified,
        'boot_status': boot_status,
        'rollback': rollback,
        'error': error,
        'last_update': datetime.datetime.now().isoformat()
    }
    
    # Log FOTA events based on status
    if chunk_received is not None:
        total_chunks = len(FIRMWARE_CHUNKS)
        progress = (chunk_received + 1) / total_chunks * 100 if total_chunks > 0 else 0
        log_fota_event(device_id, 'chunk_received', 
                      f'Chunk {chunk_received}/{total_chunks} ({progress:.1f}%)')
    
    if verified is not None:
        if verified:
            log_fota_event(device_id, 'firmware_verified', 'Hash verification successful')
        else:
            log_fota_event(device_id, 'verification_failed', f'Error: {error}')
    
    if rollback:
        log_fota_event(device_id, 'rollback_triggered', f'Reason: {error or "Verification/Boot failure"}')
        log_security_event(device_id, 'fota_rollback', f'Rolled back due to: {error}')
    
    if boot_status:
        log_fota_event(device_id, 'boot_status', f'Status: {boot_status}')
        if boot_status == 'success':
            log_fota_event(device_id, 'fota_completed', f'New firmware booted successfully')
        elif boot_status == 'failed':
            log_fota_event(device_id, 'boot_failed', f'Boot failed, rollback initiated')
    
    print(f"[FOTA STATUS] Device {device_id}: chunk {chunk_received}, verified={verified}, boot={boot_status}, rollback={rollback}")
    
    return jsonify({'status': 'success'})

@app.route('/api/cloud/fota/status', methods=['GET'])
def get_fota_status():
    """
    Get FOTA status for all devices.
    """
    return jsonify({
        'manifest': FIRMWARE_MANIFEST,
        'total_chunks': len(FIRMWARE_CHUNKS),
        'device_status': FOTA_STATUS
    })

@app.route('/api/cloud/fota/rollback', methods=['POST'])
def trigger_fota_rollback():
    """
    Trigger FOTA rollback on device.
    
    Request body:
    {
        "device_id": "optional_device_id",
        "reason": "Reason for rollback"
    }
    """
    data = request.get_json()
    device_id = data.get('device_id', 'all')
    reason = data.get('reason', 'Manual rollback requested')
    
    # Log rollback request
    log_entry = {
        'timestamp': datetime.datetime.now().isoformat(),
        'device_id': device_id,
        'action': 'rollback_requested',
        'reason': reason
    }
    FOTA_LOGS.append(log_entry)
    
    print(f"\n[FOTA ROLLBACK] Device: {device_id}")
    print(f"[FOTA ROLLBACK] Reason: {reason}")
    print(f"[FOTA ROLLBACK] Time: {log_entry['timestamp']}")
    
    # Store rollback flag for device to check on next status poll
    if device_id not in FOTA_STATUS:
        FOTA_STATUS[device_id] = {}
    
    FOTA_STATUS[device_id]['rollback_requested'] = True
    FOTA_STATUS[device_id]['rollback_reason'] = reason
    FOTA_STATUS[device_id]['rollback_timestamp'] = log_entry['timestamp']
    
    return jsonify({
        'success': True,
        'message': f'Rollback requested for device: {device_id}',
        'device_id': device_id,
        'reason': reason
    })

@app.route('/api/inverter/fota/rollback-status', methods=['GET'])
def check_rollback_status():
    """
    Device checks if rollback has been requested.
    Returns rollback flag if set.
    """
    device_id = request.headers.get('Device-ID', 'unknown')
    
    if device_id in FOTA_STATUS and FOTA_STATUS[device_id].get('rollback_requested'):
        # Clear the flag after sending
        rollback_reason = FOTA_STATUS[device_id].get('rollback_reason', 'Unknown')
        FOTA_STATUS[device_id]['rollback_requested'] = False
        
        print(f"[FOTA] Rollback flag sent to device: {device_id}")
        
        return jsonify({
            'rollback_required': True,
            'reason': rollback_reason
        })
    
    return jsonify({
        'rollback_required': False
    })

# ============ LOGGING ENDPOINTS ============

@app.route('/api/cloud/logs/security', methods=['GET'])
def get_security_logs():
    """
    Get security logs (HMAC failures, replay attacks, etc.)
    """
    device_id = request.args.get('device_id')
    limit = int(request.args.get('limit', 100))
    
    logs = SECURITY_LOGS
    if device_id:
        logs = [log for log in logs if log.get('device_id') == device_id]
    
    return jsonify({
        'total': len(logs),
        'logs': logs[-limit:]
    })

@app.route('/api/cloud/logs/fota', methods=['GET'])
def get_fota_logs():
    """
    Get FOTA operation logs (upload, download, verify, rollback)
    """
    device_id = request.args.get('device_id')
    limit = int(request.args.get('limit', 100))
    
    logs = FOTA_LOGS
    if device_id:
        logs = [log for log in logs if log.get('device_id') == device_id]
    
    return jsonify({
        'total': len(logs),
        'logs': logs[-limit:]
    })

@app.route('/api/cloud/logs/commands', methods=['GET'])
def get_command_logs():
    """
    Get command execution logs (detailed Modbus forwarding)
    """
    device_id = request.args.get('device_id')
    limit = int(request.args.get('limit', 100))
    
    logs = COMMAND_LOGS
    if device_id:
        logs = [log for log in logs if log.get('device_id') == device_id]
    
    return jsonify({
        'total': len(logs),
        'logs': logs[-limit:]
    })

@app.route('/api/cloud/logs/all', methods=['GET'])
def get_all_logs():
    """
    Get all logs for monitoring dashboard.
    """
    return jsonify({
        'security': {
            'total': len(SECURITY_LOGS),
            'recent': SECURITY_LOGS[-20:]
        },
        'fota': {
            'total': len(FOTA_LOGS),
            'recent': FOTA_LOGS[-20:]
        },
        'commands': {
            'total': len(COMMAND_LOGS),
            'recent': COMMAND_LOGS[-20:]
        }
    })

# ============ UNIFIED DEVICE COMMUNICATION ENDPOINT ============

@app.route('/api/device/status', methods=['POST'])
def device_status():
    """
    Unified endpoint for device status + receiving config/command/fota.
    Device sends: {device_id, status}
    Cloud responds with pending config, commands, FOTA info.
    """
    req = request.get_json(force=True)
    device_id = req.get('device_id', 'EcoWatt001')
    status = req.get('status', 'ready')
    
    response = {}
    
    # Check for pending config
    if device_id in PENDING_CONFIGS:
        response['config_update'] = PENDING_CONFIGS[device_id]
    
    # Check for pending command
    if device_id in PENDING_COMMANDS:
        response['command'] = PENDING_COMMANDS[device_id]
    
    # Check for FOTA
    if FIRMWARE_MANIFEST:
        # Determine next chunk to send
        device_fota = FOTA_STATUS.get(device_id, {})
        last_chunk = device_fota.get('chunk_received', -1)
        next_chunk = last_chunk + 1
        
        if next_chunk < len(FIRMWARE_CHUNKS):
            response['fota'] = {
                'manifest': FIRMWARE_MANIFEST,
                'next_chunk': next_chunk
            }
    
    return jsonify(response)

# ============ SECURITY HELPERS ============

def encrypt_payload(plaintext: str) -> str:
    """Encrypt payload using AES-256-CBC."""
    try:
        from Crypto.Cipher import AES
        from Crypto.Util.Padding import pad
        import os
        
        iv = os.urandom(16)  # Random IV
        cipher = AES.new(ENCRYPTION_KEY, AES.MODE_CBC, iv)
        padded = pad(plaintext.encode('utf-8'), AES.block_size)
        ciphertext = cipher.encrypt(padded)
        
        # Return IV + ciphertext as base64
        encrypted = base64.b64encode(iv + ciphertext).decode('ascii')
        return encrypted
    except ImportError:
        # Fallback: no encryption if pycryptodome not installed
        print("[WARNING] pycryptodome not installed, encryption disabled")
        return base64.b64encode(plaintext.encode('utf-8')).decode('ascii')

def decrypt_payload(encrypted: str) -> str:
    """Decrypt payload using AES-256-CBC."""
    try:
        from Crypto.Cipher import AES
        from Crypto.Util.Padding import unpad
        
        data = base64.b64decode(encrypted)
        iv = data[:16]
        ciphertext = data[16:]
        
        cipher = AES.new(ENCRYPTION_KEY, AES.MODE_CBC, iv)
        padded = cipher.decrypt(ciphertext)
        plaintext = unpad(padded, AES.block_size).decode('utf-8')
        return plaintext
    except ImportError:
        # Fallback: just base64 decode
        return base64.b64decode(encrypted).decode('utf-8')

def verify_hmac(payload, received_mac):
    """Verify HMAC signature."""
    calculated_mac = hmac.new(PRE_SHARED_KEY, json.dumps(payload).encode(), hashlib.sha256).hexdigest()
    return hmac.compare_digest(calculated_mac, received_mac)

def check_nonce(device_id, nonce):
    """Check nonce for anti-replay protection."""
    last_nonce = NONCE_STORE.get(device_id, 0)
    if nonce <= last_nonce:
        # Log security event
        log_security_event(device_id, 'replay_attack', f'Nonce {nonce} <= {last_nonce}')
        return False  # Replay attack
    NONCE_STORE[device_id] = nonce
    return True

def log_security_event(device_id, event_type, details):
    """Log security events."""
    SECURITY_LOGS.append({
        'timestamp': datetime.datetime.now().isoformat(),
        'device_id': device_id,
        'event_type': event_type,
        'details': details
    })
    print(f"[SECURITY] {device_id}: {event_type} - {details}")

def log_fota_event(device_id, event_type, details):
    """Log FOTA events."""
    FOTA_LOGS.append({
        'timestamp': datetime.datetime.now().isoformat(),
        'device_id': device_id,
        'event_type': event_type,
        'details': details
    })
    print(f"[FOTA] {device_id}: {event_type} - {details}")

def log_command_event(device_id, event_type, details):
    """Log command execution events."""
    COMMAND_LOGS.append({
        'timestamp': datetime.datetime.now().isoformat(),
        'device_id': device_id,
        'event_type': event_type,
        'details': details
    })
    print(f"[COMMAND] {device_id}: {event_type} - {details}")

# ============ SECURITY MONITORING ENDPOINTS ============

@app.route('/api/cloud/status', methods=['GET'])
def get_server_status():
    """
    Get current server security status and configuration.
    """
    return jsonify({
        'security_enabled': SECURITY_ENABLED,
        'nonce_window': NONCE_WINDOW,
        'nonce_expiry_seconds': NONCE_EXPIRY_SECONDS,
        'device_nonces': {k: v['nonce'] for k, v in NONCE_STORE.items()},
        'server_nonce_counter': SERVER_NONCE_COUNTER,
        'total_security_events': len(SECURITY_LOGS),
        'total_devices_active': len(NONCE_STORE)
    })

@app.route('/api/cloud/security/clear', methods=['POST'])
def clear_security_logs():
    """
    Clear security logs (for demo purposes).
    """
    global SECURITY_LOGS
    SECURITY_LOGS = []
    return jsonify({'status': 'success', 'message': 'Security logs cleared'})

# ---------------- Startup ----------------

def _start_flusher_once():
    t = threading.Thread(target=_flusher_loop, daemon=True)
    t.start()

_start_flusher_once()

# Load persisted configurations on startup
print("[CONFIG] Loading persisted configurations...")
load_pending_configs()
load_config_history()
print(f"[CONFIG] Ready: {len(PENDING_CONFIGS)} pending configs, {len(CONFIG_HISTORY)} history entries")

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
