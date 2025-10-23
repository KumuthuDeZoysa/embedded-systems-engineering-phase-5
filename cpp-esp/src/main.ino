
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include "../include/ecoWatt_device.hpp"
#include "../include/logger.hpp"

EcoWattDevice device;

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
}

void loop() {
    device.loop();
    // Print any available serial data to the monitor
    while (Serial.available()) {
        String data = Serial.readStringUntil('\n');
        Serial.print("[SERIAL DATA] ");
        Serial.println(data);
    }
}
