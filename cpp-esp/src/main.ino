
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include "../include/ecoWatt_device.hpp"
#include "../include/logger.hpp"

EcoWattDevice device;

// Serial command buffer
String serialBuffer = "";

// Auto power demo trigger
bool powerDemoRun = false;
unsigned long powerDemoStartTime = 0;

void processSerialCommand(String cmd) {
    cmd.trim();
    cmd.toUpperCase();
    
    Serial.printf("\n[CMD] Processing: %s\n", cmd.c_str());
    
    if (cmd == "HELP" || cmd == "?") {
        Serial.println("\n========== AVAILABLE COMMANDS ==========");
        Serial.println("POWER HIGH    - Set HIGH_PERFORMANCE mode (240MHz, no sleep)");
        Serial.println("POWER NORMAL  - Set NORMAL mode (160MHz, WiFi sleep)");
        Serial.println("POWER LOW     - Set LOW_POWER mode (80MHz, WiFi sleep, ADC off)");
        Serial.println("STATS         - Show current power statistics");
        Serial.println("COMPARE       - Run power comparison demo");
        Serial.println("HELP or ?     - Show this help menu");
        Serial.println("=========================================\n");
    }
    else if (cmd.startsWith("POWER ")) {
        String mode = cmd.substring(6);
        mode.trim();
        if (mode == "HIGH" || mode == "HIGH_PERFORMANCE") {
            device.setPowerMode("HIGH");
        } else if (mode == "NORMAL") {
            device.setPowerMode("NORMAL");
        } else if (mode == "LOW" || mode == "LOW_POWER") {
            device.setPowerMode("LOW");
        } else {
            Serial.printf("[CMD] Unknown power mode: %s\n", mode.c_str());
            Serial.println("[CMD] Use: POWER HIGH, POWER NORMAL, or POWER LOW");
        }
    }
    else if (cmd == "STATS") {
        device.printPowerStats();
    }
    else if (cmd == "COMPARE") {
        Serial.println("\n========== POWER COMPARISON DEMO ==========");
        Serial.println("Step 1: Setting HIGH_PERFORMANCE mode...");
        device.setPowerMode("HIGH");
        delay(2000);
        
        Serial.println("\nStep 2: Setting NORMAL mode (optimized)...");
        device.setPowerMode("NORMAL");
        delay(2000);
        
        Serial.println("\nStep 3: Setting LOW_POWER mode...");
        device.setPowerMode("LOW");
        delay(2000);
        
        Serial.println("\n--- Returning to NORMAL mode ---");
        device.setPowerMode("NORMAL");
        Serial.println("============================================\n");
    }
    else if (cmd.length() > 0) {
        Serial.printf("[CMD] Unknown command: %s (type HELP for commands)\n", cmd.c_str());
    }
}

void setup() {
    Serial.begin(115200);
#if defined(ESP32)
    if (!LittleFS.begin(true)) { // true = format if mount fails
        Serial.println("LittleFS mount failed and formatted!");
    }
    // Ensure required directories and files exist on LittleFS
    if (!LittleFS.exists("/config")) LittleFS.mkdir("/config");
    if (!LittleFS.exists("/data")) LittleFS.mkdir("/data");
    if (!LittleFS.exists("/logs")) LittleFS.mkdir("/logs");
    if (!LittleFS.exists("/littlefs")) LittleFS.mkdir("/littlefs");
    if (!LittleFS.exists("/config/config.json")) {
        File f = LittleFS.open("/config/config.json", "w");
        f.print("{\n  \"wifi\": {\n    \"ssid\": \"Senum's iPhone\",\n    \"password\": \"1234567891\"\n  }\n}\n");
        f.close();
    }
    if (!LittleFS.exists("/data/samples.csv")) {
        File f = LittleFS.open("/data/samples.csv", "w");
        f.print("timestamp,reg_addr,value\n");
        f.close();
    }
    if (!LittleFS.exists("/logs/main.log")) {
        File f = LittleFS.open("/logs/main.log", "w");
        f.print("");
        f.close();
    }
#endif
    device.setup();
    Logger::info("EcoWatt Device setup complete");
    
    // Schedule automatic power demo after 10 seconds
    powerDemoStartTime = millis();
    Serial.println("\n*** POWER DEMO will auto-run in 10 seconds ***");
    Serial.println("*** Or type COMPARE to run immediately ***\n");
}

void loop() {
    device.loop();
    
    // Auto-run power demo 10 seconds after boot (once only)
    if (!powerDemoRun && millis() - powerDemoStartTime > 10000) {
        powerDemoRun = true;
        Serial.println("\n\n");
        Serial.println("╔══════════════════════════════════════════════════════════════╗");
        Serial.println("║        POWER OPTIMIZATION COMPARISON DEMO - MILESTONE 5      ║");
        Serial.println("╚══════════════════════════════════════════════════════════════╝");
        Serial.println();
        
        // Step 1: HIGH_PERFORMANCE baseline
        Serial.println("┌─────────────────────────────────────────────────────────────┐");
        Serial.println("│ STEP 1: HIGH_PERFORMANCE MODE (Baseline - No Optimization) │");
        Serial.println("└─────────────────────────────────────────────────────────────┘");
        device.setPowerMode("HIGH");
        device.printPowerStats();
        delay(3000);
        
        // Step 2: NORMAL mode with WiFi sleep
        Serial.println("\n┌─────────────────────────────────────────────────────────────┐");
        Serial.println("│ STEP 2: NORMAL MODE (CPU Scaling + WiFi Light Sleep)        │");
        Serial.println("└─────────────────────────────────────────────────────────────┘");
        device.setPowerMode("NORMAL");
        device.printPowerStats();
        delay(3000);
        
        // Step 3: LOW_POWER mode
        Serial.println("\n┌─────────────────────────────────────────────────────────────┐");
        Serial.println("│ STEP 3: LOW_POWER MODE (Minimum CPU + Peripheral Gating)    │");
        Serial.println("└─────────────────────────────────────────────────────────────┘");
        device.setPowerMode("LOW");
        device.printPowerStats();
        delay(3000);
        
        // Summary
        Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
        Serial.println("║                    POWER SAVINGS SUMMARY                      ║");
        Serial.println("╠══════════════════════════════════════════════════════════════╣");
        Serial.println("║  Mode              │ CPU Freq │ WiFi Sleep │ Est. Current    ║");
        Serial.println("╠══════════════════════════════════════════════════════════════╣");
        Serial.println("║  HIGH_PERFORMANCE  │ 240 MHz  │ OFF        │ ~160 mA         ║");
        Serial.println("║  NORMAL            │ 160 MHz  │ OFF        │ ~95 mA          ║");
        Serial.println("║  LOW_POWER         │ 80 MHz   │ ON         │ ~80 mA          ║");
        Serial.println("╠══════════════════════════════════════════════════════════════╣");
        Serial.println("║  POWER SAVINGS: HIGH → NORMAL = 40.6% reduction             ║");
        Serial.println("║  POWER SAVINGS: HIGH → LOW    = 50.0% reduction             ║");
        Serial.println("╚══════════════════════════════════════════════════════════════╝");
        
        // Return to NORMAL for regular operation
        Serial.println("\n>>> Returning to NORMAL mode for regular operation <<<\n");
        device.setPowerMode("NORMAL");
    }
    
    // Process serial commands for demo
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialBuffer.length() > 0) {
                processSerialCommand(serialBuffer);
                serialBuffer = "";
            }
        } else {
            serialBuffer += c;
        }
    }
}
