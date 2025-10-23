# Milestone 4 - Part 1: Remote Configuration Implementation

## Overview
This document describes the implementation of runtime remote configuration for the EcoWatt Device. The system allows the cloud to update device parameters without requiring reboot or reflashing.

## Architecture

### Components

#### 1. Device Side (ESP32/NodeMCU)
- **ConfigManager**: Validates, applies, and persists configuration updates
- **RemoteConfigHandler**: Polls cloud for updates and sends acknowledgments
- **AcquisitionScheduler**: Applies configuration changes to acquisition process
- **EcoWattDevice**: Coordinates configuration updates across subsystems

#### 2. Cloud Side (Flask Server)
- **Configuration Endpoints**: Manage configuration updates and acknowledgments
- **Event Logging**: Track all configuration changes with timestamps
- **State Management**: Maintain current and pending configurations

## Supported Configuration Parameters

### 1. Sampling Interval
- **Parameter Name**: `sampling_interval`
- **Unit**: Seconds (converted to milliseconds internally)
- **Valid Range**: 1-300 seconds (1 second to 5 minutes)
- **Default**: 5 seconds

### 2. Register List
- **Parameter Name**: `registers`
- **Format**: Array of register names or addresses
- **Valid Range**: 1-10 registers
- **Valid Addresses**: 0-9
- **Supported Register Names**:
  - `voltage` → Address 0 (Vac1_L1_Phase_voltage)
  - `current` → Address 1 (Iac1_L1_Phase_current)
  - `frequency` → Address 2 (Fac1_L1_Phase_frequency)
  - `pv1_voltage` → Address 3 (Vpv1_PV1_input_voltage)
  - `pv2_voltage` → Address 4 (Vpv2_PV2_input_voltage)
  - `pv1_current` → Address 5 (Ipv1_PV1_input_current)
  - `pv2_current` → Address 6 (Ipv2_PV2_input_current)
  - `temperature` → Address 7 (Inverter_internal_temperature)
  - `export_power` → Address 8 (Export_power_percentage)
  - `output_power` → Address 9 (Pac_L_Inverter_output_power)

## Message Formats

### Cloud → Device: Configuration Update Request

```json
{
  "nonce": 1001,
  "config_update": {
    "sampling_interval": 10,
    "registers": ["voltage", "current", "frequency"]
  }
}
```

### Device → Cloud: Configuration Acknowledgment

```json
{
  "nonce": 1001,
  "timestamp": 123456789,
  "all_success": true,
  "config_ack": {
    "accepted": [
      {
        "parameter": "sampling_interval",
        "old_value": "5000",
        "new_value": "10000",
        "reason": "Applied successfully"
      },
      {
        "parameter": "registers",
        "old_value": "0,1,2,3,4,5,6,7,8,9",
        "new_value": "0,1,2",
        "reason": "Applied successfully"
      }
    ],
    "rejected": [],
    "unchanged": []
  }
}
```

## API Endpoints

### 1. GET /api/inverter/config
**Purpose**: Device polls for pending configuration updates

**Request**: None

**Response** (with pending update):
```json
{
  "nonce": 1001,
  "config_update": {
    "sampling_interval": 10,
    "registers": ["voltage", "current"]
  }
}
```

**Response** (no updates):
```json
{
  "message": "no updates"
}
```

### 2. POST /api/inverter/config/ack
**Purpose**: Device sends acknowledgment after processing update

**Request**: Configuration acknowledgment (see message format above)

**Response**:
```json
{
  "status": "acknowledged",
  "message": "Configuration acknowledgment received and processed"
}
```

### 3. POST /api/config/update
**Purpose**: Manual trigger for configuration update (testing/admin)

**Request**:
```json
{
  "sampling_interval": 10,
  "registers": ["voltage", "current", "frequency"]
}
```

**Response**:
```json
{
  "status": "queued",
  "message": "Configuration update queued for device",
  "nonce": 1001,
  "config_update": {
    "sampling_interval": 10,
    "registers": ["voltage", "current", "frequency"]
  }
}
```

### 4. GET /api/config/status
**Purpose**: Get current configuration status and history

**Response**:
```json
{
  "current_device_config": {
    "sampling_interval": 5,
    "registers": ["voltage", "current", "frequency", "pv1_voltage", ...]
  },
  "pending_update": null,
  "current_nonce": 1001,
  "update_history_count": 5,
  "ack_history_count": 5,
  "recent_updates": [...],
  "recent_acks": [...]
}
```

## Validation Rules

### Sampling Interval
- Minimum: 1000 ms (1 second)
- Maximum: 300000 ms (5 minutes)
- Rejection reason examples:
  - "Sampling interval too low (min: 1000 ms)"
  - "Sampling interval too high (max: 300000 ms)"

### Register List
- Minimum count: 1 register
- Maximum count: 10 registers
- Valid addresses: 0-9
- No duplicates allowed
- All registers must exist in system
- Rejection reason examples:
  - "Register count too low (min: 1)"
  - "Register count too high (max: 10)"
  - "Invalid register address: 15 (valid range: 0-9)"
  - "Register 5 not defined in system"
  - "Duplicate register address: 2"

## Idempotency and Deduplication

### Nonce Management
- Each configuration update has a unique nonce
- Device stores last processed nonce in persistent storage
- Duplicate requests (same nonce) are automatically detected
- Response for duplicate: `"unchanged"` with reason "Duplicate request (nonce already processed)"

### Value Comparison
- Before applying changes, device compares new values with current values
- If values are identical, parameter is marked as `"unchanged"`
- This prevents unnecessary updates and storage writes

## Persistence

### Device Side
- Configuration stored in LittleFS: `/config/persistent.dat`
- Binary format for efficiency
- Includes checksum for integrity verification
- Survives power cycles and reboots
- Format:
  ```
  version (uint32_t)
  sampling_interval_ms (uint32_t)
  register_count (uint32_t)
  registers (uint8_t array)
  last_nonce (uint32_t)
  last_update_timestamp (uint32_t)
  checksum (uint32_t)
  ```

### Cloud Side
- All events logged to `config_events.json`
- Each event includes:
  - Timestamp (ISO 8601 format)
  - Event type (CONFIG_UPDATE_REQUESTED, CONFIG_ACK_RECEIVED)
  - Event data
- In-memory state maintained for active session
- Event log can be parsed for auditing and reporting

## Thread Safety

### Non-Blocking Updates
- Configuration polling runs in background (every 60 seconds)
- Updates are applied between acquisition cycles
- AcquisitionScheduler's `updateConfig()` method is thread-safe
- No interruption to ongoing data acquisition

### Critical Sections
- Configuration updates use atomic operations where possible
- Ticker intervals updated safely without stopping acquisition
- Register list updated as complete array (no partial updates)

## Error Handling

### Validation Errors
- Invalid parameters are rejected immediately
- Reason provided in acknowledgment
- Valid parameters in same request are still applied
- `all_success` flag set to `false` if any rejections

### Network Errors
- Device retries on connection failure (built into HTTP client)
- Pending updates remain in cloud until acknowledged
- Device polls regularly (60-second interval)

### Storage Errors
- Checksum verification on load
- Corrupted config files trigger default initialization
- Save failures are logged but don't block operation

## Testing Scenarios

### Test 1: Valid Sampling Interval Update
```bash
# Trigger update
curl -X POST http://localhost:8080/api/config/update \
  -H "Content-Type: application/json" \
  -d '{"sampling_interval": 10}'

# Device will poll and receive update
# Device applies update
# Device sends acknowledgment
# Result: sampling_interval changed from 5s to 10s
```

### Test 2: Valid Register List Update
```bash
curl -X POST http://localhost:8080/api/config/update \
  -H "Content-Type: application/json" \
  -d '{"registers": ["voltage", "current", "frequency"]}'

# Result: Only 3 registers polled instead of 10
```

### Test 3: Invalid Sampling Interval (Rejected)
```bash
curl -X POST http://localhost:8080/api/config/update \
  -H "Content-Type: application/json" \
  -d '{"sampling_interval": 500}'

# Result: Cloud rejects (too high)
```

### Test 4: Duplicate Request (Idempotent)
```bash
# Send same update twice with same parameters
# Second update results in "unchanged" response
```

### Test 5: Power Cycle Test
```bash
# 1. Apply configuration update
# 2. Power cycle device (reset)
# 3. Verify configuration persists after reboot
# Result: Device uses updated configuration after restart
```

### Test 6: Mixed Valid/Invalid Update
```bash
curl -X POST http://localhost:8080/api/config/update \
  -H "Content-Type: application/json" \
  -d '{"sampling_interval": 10, "registers": ["invalid_reg"]}'

# Result: sampling_interval accepted, registers rejected
# Acknowledgment shows both outcomes
```

## Usage Instructions

### Starting the Cloud Server

1. Install dependencies:
```bash
pip install flask flask-cors
```

2. Run the server:
```bash
python cloud_server.py
```

3. Server will start on `http://0.0.0.0:8080`

### Testing with Test Client

```bash
python test_config_client.py
```

This runs a complete test scenario including:
- Health check
- Current status query
- Sampling interval update
- Register list update
- Invalid updates
- Full acknowledgment flow

### Device Configuration

Update `config.json` with cloud endpoint:
```json
{
  "api": {
    "config_endpoint": "/api/inverter/config"
  }
}
```

### Manual Testing with curl

```bash
# Get current status
curl http://localhost:8080/api/config/status

# Trigger config update
curl -X POST http://localhost:8080/api/config/update \
  -H "Content-Type: application/json" \
  -d '{"sampling_interval": 10, "registers": ["voltage", "current"]}'

# Check pending update (what device will see)
curl http://localhost:8080/api/inverter/config
```

## Logging

### Device Logs
Located in `/logs/main.log` on device (LittleFS)

Example entries:
```
[INFO] [ConfigMgr] Processing config update request, nonce=1001
[INFO] [ConfigMgr] Sampling interval updated: 5000 -> 10000 ms
[INFO] [ConfigMgr] Register list updated: [0,1,2,3,4,5,6,7,8,9] -> [0,1,2]
[INFO] [ConfigMgr] Configuration persisted to flash
[INFO] [RemoteCfg] Config acknowledgment sent successfully
[INFO] [EcoWattDevice] Applied config to scheduler: interval=10000 ms, registers=3
```

### Cloud Logs
Located in `cloud_server.log` and console output

Example entries:
```
2025-10-10 14:30:15 [INFO] [CONFIG_UPDATE_REQUESTED] {"nonce": 1001, "requested_config": {...}}
2025-10-10 14:30:45 [INFO] [GET /api/inverter/config] Device polling for config updates
2025-10-10 14:31:00 [INFO] [CONFIG_ACK_RECEIVED] {"nonce": 1001, "all_success": true, ...}
2025-10-10 14:31:00 [INFO] Device config updated: sampling_interval = 10000 ms
```

## Performance Characteristics

- **Config Update Latency**: 0-60 seconds (depends on polling interval)
- **Memory Overhead**: ~500 bytes for persistent config storage
- **Network Overhead**: ~300-500 bytes per config exchange
- **CPU Impact**: Minimal (validation and persistence < 10ms)
- **Flash Wear**: 1 write per successful update (typical: < 10/day)

## Future Enhancements

1. **Push Notifications**: Use MQTT for instant config delivery instead of polling
2. **Batch Updates**: Support multiple configuration changes in queue
3. **Rollback**: Automatic rollback if device becomes unstable after update
4. **Versioning**: Track configuration version history
5. **Conditional Updates**: Apply updates based on device state/conditions
6. **Remote Validation**: Cloud pre-validates before sending to device

## Troubleshooting

### Device Not Receiving Updates
1. Check device WiFi connection
2. Verify cloud endpoint URL in config
3. Check device logs for polling activity
4. Verify pending update exists in cloud

### Updates Rejected
1. Check validation rules
2. Review device logs for specific rejection reasons
3. Verify parameter values are within valid ranges

### Updates Not Persisting
1. Check LittleFS mount status
2. Verify write permissions
3. Check for checksum errors in logs
4. Ensure sufficient flash space

### Acknowledgment Not Received
1. Check network connectivity
2. Verify cloud endpoint for acknowledgments
3. Check HTTP client logs for errors
4. Verify JSON format of acknowledgment

## Summary

This implementation provides a complete, production-ready remote configuration system that:
- ✅ Updates parameters at runtime without reboot
- ✅ Validates all parameters before applying
- ✅ Persists configuration across power cycles
- ✅ Handles idempotent updates gracefully
- ✅ Provides detailed acknowledgments
- ✅ Maintains complete audit logs
- ✅ Supports thread-safe, non-blocking updates
- ✅ Includes comprehensive error handling
