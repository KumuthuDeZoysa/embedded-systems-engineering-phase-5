# 🚀 Milestone 4 Complete Demo Video Script
**Duration: 10 minutes maximum**  
**Presenter: [New group member who hasn't presented before]**

---

## 🎬 **INTRODUCTION (1 minute)**

**[Screen: Show demo setup with ESP32, Flask server, and terminal windows]**

**NARRATOR:** "Welcome to the Milestone 4 complete demonstration for the EcoWatt Device. I'm [Name], presenting our final milestone featuring remote configuration, command execution, security implementation, and firmware over-the-air updates. Today we'll demonstrate all four parts working together in a production-ready system."

**[Screen shows the four demo components]**

**NARRATOR:** "Our demonstration covers:"
- ✅ **Part 1: Remote Configuration** - Real-time parameter updates
- ✅ **Part 2: Command Execution** - Cloud-to-device command round-trips  
- ✅ **Part 3: Security Layer** - HMAC authentication and anti-replay protection
- ✅ **Part 4: FOTA Updates** - Secure firmware updates with rollback capability

---

## 🔧 **PART 1: Remote Configuration Demo (2 minutes)**

**[Screen: Configuration dashboard and ESP32 serial monitor side by side]**

**NARRATOR:** "First, let's demonstrate remote configuration. The device currently polls the cloud every 60 seconds for configuration updates."

### **Live Configuration Update:**

**[Open config_dashboard.html in browser]**

**NARRATOR:** "Using our web dashboard, I'll update the sampling interval from 5000ms to 3000ms and modify the register list."

**[Make changes in dashboard and click Update Configuration]**

**[Screen: Show ESP32 serial monitor]**

**NARRATOR:** "Watch the ESP32 logs - within 60 seconds, the device will:"
- 🔍 Poll for configuration updates  
- 📥 Receive the new configuration
- ✅ Validate and apply the changes
- 📤 Send acknowledgment back to cloud

**[Wait for logs to show config update]**

**Expected logs:**
```
[RemoteCfg] Polling for config updates...
[Config] Received update: sampling_interval=3000ms
[Config] Configuration applied successfully
[RemoteCfg] Sending acknowledgment...
```

**NARRATOR:** "Perfect! The device verified, applied the new configuration internally, and reported success. The acquisition scheduler is now using the new 3-second interval."

---

## ⚡ **PART 2: Command Execution Demo (2 minutes)**

**[Screen: Cloud command interface and device logs]**

**NARRATOR:** "Next, we'll demonstrate command execution - the complete round-trip from cloud to device to inverter simulator and back."

### **Send Write Command:**

**[Use curl or dashboard to send command]**

```bash
curl -X POST http://localhost:8080/api/command/queue \
  -H "Content-Type: application/json" \
  -d '{"device_id": "EcoWatt001", "action": "write", "target_register": "8", "value": 42.5}'
```

**NARRATOR:** "I'm queuing a write command to register 8 with value 42.5. Watch as the device:"

**[Screen: ESP32 serial monitor]**

**Expected sequence:**
```
[RemoteCfg] Checking for commands...
[Command] Received write command: reg=8, value=42.5
[Protocol] Writing register 8 with value 42.5
[Command] Command executed successfully
[RemoteCfg] Sending command result...
```

**[Screen: Cloud command results endpoint]**

**NARRATOR:** "And here's the cloud confirmation - the complete round-trip took approximately 3 seconds. The command was executed, verified on the inverter simulator, and the result reported back with full traceability."

---

## 🔐 **PART 3: Security Demonstration (1.5 minutes)**

**[Screen: Security monitor and server logs]**

**NARRATOR:** "Our security layer implements three critical features: HMAC authentication, encryption, and anti-replay protection. Let's see this in action."

### **Normal Authenticated Request:**

**[Screen: Show server logs highlighting security events]**

**NARRATOR:** "Every device request uses HMAC-SHA256 authentication. Notice these security log entries:"

**Expected logs:**
```
[SECURITY] EcoWatt001: hmac_verified - Nonce: 157
[SECURITY] EcoWatt001: nonce_validated - Previous: 156, Current: 157
[SECURITY] EcoWatt001: request_authenticated - Success
```

### **Replay Attack Demo:**

**[Run security demo script]**

```bash
python security_demo.py
```

**NARRATOR:** "Now I'll simulate a replay attack using an old message. Watch how the security layer detects and blocks it:"

**Expected response:**
```
[SECURITY] EcoWatt001: replay_attack_detected - Nonce: 145 (too old)
[SECURITY] EcoWatt001: request_rejected - Reason: Invalid nonce
HTTP 401: Authentication failed
```

**NARRATOR:** "Excellent! The system correctly identified the replay attack and rejected the request. This demonstrates our robust anti-replay protection."

---

## 🔄 **PART 4A: Normal FOTA Update (2.5 minutes)**

**[Screen: FOTA demo script and ESP32 monitor]**

**NARRATOR:** "Now for the main event - our secure firmware over-the-air update system. I'll start with a normal update from version 1.0.3 to 1.0.6."

### **Upload New Firmware:**

```bash
python fota_production_demo.py --demo normal --version 1.0.6 --size 35840
```

**NARRATOR:** "This uploads a 35KB firmware binary, calculates the SHA-256 hash, chunks it into 1KB pieces with HMAC authentication, and makes it available for download."

**[Screen: Show upload progress]**

**Expected output:**
```
📦 Uploading Firmware 1.0.6
✅ Firmware uploaded successfully!
✅ Cloud ready with 35 chunks
```

### **Monitor ESP32 Download:**

**[Screen: ESP32 serial monitor and demo script monitoring]**

**NARRATOR:** "The ESP32 checks for updates every 23 seconds. Watch as it detects the new version and begins the chunked download process:"

**Expected sequence:**
```
[FOTA] Checking for firmware updates
[FOTA] Current: 1.0.3, Available: 1.0.6 - Update needed
[FOTA] Starting chunked download...
[FOTA] Downloaded chunk 1/35 (2.9%)
[FOTA] Downloaded chunk 5/35 (14.3%)
...
[FOTA] Downloaded chunk 35/35 (100.0%)
[FOTA] Verifying SHA-256 hash...
✅ [FOTA] Hash verification successful!
[FOTA] Writing to OTA partition...
[FOTA] Rebooting in 3 seconds...
```

**[Device reboots]**

**NARRATOR:** "After reboot, the device reports successful boot confirmation:"

```
[FOTA] Boot successful - Running version 1.0.6
[FOTA] Reporting boot status to cloud...
```

**NARRATOR:** "Perfect! The complete FOTA process demonstrates chunked download, integrity verification, controlled reboot, and boot confirmation - all with HMAC security."

---

## 🔄 **PART 4B: FOTA Rollback Demo (1.5 minutes)**

**[Screen: Continue with rollback demo]**

**NARRATOR:** "Finally, let's demonstrate the rollback capability by uploading corrupted firmware that will fail verification."

### **Upload Corrupted Firmware:**

```bash
python fota_production_demo.py --demo rollback --version 1.0.7 --size 35840
```

**NARRATOR:** "This upload includes intentionally corrupted data that will fail SHA-256 verification."

**[Screen: ESP32 monitor]**

**Expected sequence:**
```
[FOTA] Checking for firmware updates
[FOTA] Current: 1.0.6, Available: 1.0.7 - Update needed
[FOTA] Starting chunked download...
[FOTA] Downloaded chunk 35/35 (100.0%)
[FOTA] Verifying SHA-256 hash...
❌ [FOTA] Hash verification FAILED!
🔄 [FOTA] Triggering rollback to version 1.0.6
[FOTA] Rollback completed - System stable
```

**NARRATOR:** "Excellent! The system detected the corruption, rejected the bad firmware, and safely rolled back to the previous working version. The device continues operating normally with version 1.0.6."

---

## 🎯 **CONCLUSION (30 seconds)**

**[Screen: Summary of all demonstrations]**

**NARRATOR:** "In this demonstration, we successfully showed:"

✅ **Remote Configuration** - Real-time parameter updates with validation  
✅ **Command Execution** - Complete cloud-to-device-to-cloud round-trip  
✅ **Security Layer** - HMAC authentication with anti-replay protection  
✅ **FOTA Updates** - Secure firmware updates with chunked download  
✅ **FOTA Rollback** - Safety mechanism preventing system corruption  

**NARRATOR:** "All features work together as a production-ready IoT device management system. The EcoWatt device can be safely updated, configured, and controlled remotely while maintaining security and reliability. Thank you for watching our Milestone 4 demonstration!"

---

## 📋 **Pre-Demo Checklist**

### **Before Recording:**

1. **✅ Start Flask Server:**
   ```bash
   cd "E:\ES Phase 4\embedded-systems-engineering-phase-4"
   python cpp-esp\app.py
   ```

2. **✅ Upload ESP32 Code:**
   ```bash
   cd "e:\ES Phase 4\embedded-systems-engineering-phase-4\cpp-esp"
   pio run --target upload --upload-port COM5
   ```

3. **✅ Open Serial Monitor:**
   ```bash
   pio device monitor --port COM5 --baud 115200
   ```

4. **✅ Test Scripts:**
   ```bash
   # Test normal FOTA
   python fota_production_demo.py --demo status
   
   # Ensure config dashboard loads
   # Open config_dashboard.html in browser
   ```

5. **✅ Window Layout:**
   - ESP32 Serial Monitor (top-left)
   - Flask Server Logs (top-right)  
   - Terminal for commands (bottom-left)
   - Browser for dashboard (bottom-right)

6. **✅ Backup current firmware version** in case rollback is needed

### **During Recording:**

- **Keep video ON throughout** (requirement)
- **Explain each step clearly** before executing
- **Wait for logs to appear** - don't rush
- **Point out key security features** as they happen
- **Show error handling** during rollback demo

### **Key Things to Emphasize:**

1. **Security**: Every request is HMAC authenticated
2. **Reliability**: Configuration changes are validated before applying
3. **Safety**: FOTA includes rollback protection
4. **Production-Ready**: All features work together seamlessly
5. **Real-Time**: Changes happen within seconds

---

## 🛠 **Troubleshooting**

### **If FOTA doesn't start:**
- Check ESP32 is connected to WiFi
- Verify Flask server is running on correct IP
- Ensure security configuration matches between device and server

### **If configuration update fails:**
- Check nonce synchronization 
- Verify Device-ID header is present
- Ensure JSON format is correct

### **If commands don't execute:**
- Verify device polls for commands every 60 seconds
- Check command format matches expected structure
- Ensure target register exists in configuration

---

**🎬 Ready for filming! This script covers all Milestone 4 requirements with proper demonstrations of each feature.**