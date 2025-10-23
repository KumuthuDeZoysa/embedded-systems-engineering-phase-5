/**
 * Milestone 4 Part 3 - Security Layer Test Program
 * 
 * This is a STANDALONE test program to verify security implementation.
 * Upload this to ESP32 to test security features before integrating with main code.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "security_layer.hpp"
#include "config_manager.hpp"

// Test Configuration
const char* TEST_SERVER_URL = "http://192.168.1.152:8080/api/security/test";
const char* DEVICE_ID = "ESP32-TEST-001";

// Global objects
ConfigManager* config;
SecurityLayer* security;

// Test Results
int tests_passed = 0;
int tests_failed = 0;

void printTestHeader(const char* testName) {
    Serial.println();
    Serial.println("========================================");
    Serial.println(testName);
    Serial.println("========================================");
}

void printTestResult(const char* testName, bool passed) {
    if (passed) {
        Serial.print("✓ PASS: ");
        tests_passed++;
    } else {
        Serial.print("✗ FAIL: ");
        tests_failed++;
    }
    Serial.println(testName);
}

/**
 * TEST 1: Security Layer Initialization
 */
bool test_initialization() {
    printTestHeader("TEST 1: Security Layer Initialization");
    
    Serial.println("[1.1] Loading configuration...");
    config = new ConfigManager();
    if (!config->begin()) {
        Serial.println("ERROR: Config load failed!");
        return false;
    }
    Serial.println("✓ Config loaded");
    
    Serial.println("[1.2] Initializing security layer...");
    security = new SecurityLayer(config);
    if (!security->begin()) {
        Serial.println("ERROR: Security init failed!");
        return false;
    }
    Serial.println("✓ Security initialized");
    
    Serial.println("[1.3] Checking PSK...");
    Serial.print("  PSK length: ");
    Serial.println(config->security.psk.length());
    Serial.print("  PSK (first 16 chars): ");
    Serial.println(config->security.psk.substring(0, 16));
    
    return true;
}

/**
 * TEST 2: HMAC Computation
 */
bool test_hmac() {
    printTestHeader("TEST 2: HMAC Computation");
    
    const char* testData = "Hello World";
    String mac = security->computeHMAC(testData, config->security.psk.c_str());
    
    Serial.println("[2.1] Computing HMAC...");
    Serial.print("  Input: ");
    Serial.println(testData);
    Serial.print("  HMAC: ");
    Serial.println(mac);
    Serial.print("  Length: ");
    Serial.println(mac.length());
    
    // Verify HMAC length
    if (mac.length() != 64) {
        Serial.println("ERROR: HMAC should be 64 characters!");
        return false;
    }
    
    // Verify HMAC is hex
    for (int i = 0; i < mac.length(); i++) {
        char c = mac.charAt(i);
        if (!isxdigit(c)) {
            Serial.println("ERROR: HMAC should be hex!");
            return false;
        }
    }
    
    Serial.println("✓ HMAC format correct");
    
    // Test consistency
    String mac2 = security->computeHMAC(testData, config->security.psk.c_str());
    if (mac != mac2) {
        Serial.println("ERROR: HMAC not consistent!");
        return false;
    }
    Serial.println("✓ HMAC consistent");
    
    return true;
}

/**
 * TEST 3: Nonce Generation
 */
bool test_nonce() {
    printTestHeader("TEST 3: Nonce Generation");
    
    Serial.println("[3.1] Generating nonces...");
    uint32_t nonces[5];
    for (int i = 0; i < 5; i++) {
        nonces[i] = security->getNextNonce();
        Serial.print("  Nonce ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.println(nonces[i]);
    }
    
    // Verify nonces are sequential
    for (int i = 1; i < 5; i++) {
        if (nonces[i] != nonces[i-1] + 1) {
            Serial.println("ERROR: Nonces not sequential!");
            return false;
        }
    }
    Serial.println("✓ Nonces sequential");
    
    return true;
}

/**
 * TEST 4: Secure Message Creation
 */
bool test_secure_message() {
    printTestHeader("TEST 4: Secure Message Creation");
    
    Serial.println("[4.1] Creating test payload...");
    const char* plainJson = "{\"test\":\"security\",\"value\":42}";
    Serial.print("  Plain JSON: ");
    Serial.println(plainJson);
    
    Serial.println("[4.2] Securing message...");
    SecuredMessage secured;
    SecurityResult result = security->secureMessage(plainJson, secured);
    
    if (!result.is_success()) {
        Serial.print("ERROR: secureMessage failed with status: ");
        Serial.println((int)result.status);
        return false;
    }
    
    Serial.println("✓ Message secured");
    Serial.print("  Nonce: ");
    Serial.println(secured.nonce);
    Serial.print("  Timestamp: ");
    Serial.println(secured.timestamp);
    Serial.print("  Encrypted: ");
    Serial.println(secured.encrypted);
    Serial.print("  Payload (Base64): ");
    Serial.println(secured.payload);
    Serial.print("  MAC: ");
    Serial.println(secured.mac);
    
    return true;
}

/**
 * TEST 5: Round-trip Verification
 */
bool test_roundtrip() {
    printTestHeader("TEST 5: Round-trip Verification");
    
    const char* original = "{\"type\":\"test\",\"data\":\"roundtrip\"}";
    Serial.print("[5.1] Original: ");
    Serial.println(original);
    
    // Secure the message
    SecuredMessage secured;
    SecurityResult result1 = security->secureMessage(original, secured);
    if (!result1.is_success()) {
        Serial.println("ERROR: Failed to secure message");
        return false;
    }
    Serial.println("✓ Message secured");
    
    // Generate envelope
    String envelope = security->generateSecuredEnvelope(secured);
    Serial.println("[5.2] Envelope generated");
    Serial.println(envelope);
    
    // Verify the message
    String recovered;
    SecurityResult result2 = security->verifyMessage(envelope.c_str(), recovered);
    if (!result2.is_success()) {
        Serial.print("ERROR: Verification failed with status: ");
        Serial.println((int)result2.status);
        return false;
    }
    
    Serial.print("[5.3] Recovered: ");
    Serial.println(recovered);
    
    // Compare
    if (String(original) != recovered) {
        Serial.println("ERROR: Original doesn't match recovered!");
        Serial.println("Original: " + String(original));
        Serial.println("Recovered: " + recovered);
        return false;
    }
    
    Serial.println("✓ Round-trip successful!");
    return true;
}

/**
 * TEST 6: Server Communication
 */
bool test_server_communication() {
    printTestHeader("TEST 6: Server Communication");
    
    Serial.println("[6.1] Checking WiFi connection...");
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("ERROR: WiFi not connected!");
        return false;
    }
    Serial.print("✓ WiFi connected: ");
    Serial.println(WiFi.localIP());
    
    Serial.println("[6.2] Creating test message...");
    const char* testPayload = "{\"type\":\"server_test\",\"message\":\"Hello from ESP32\"}";
    
    SecuredMessage secured;
    SecurityResult result = security->secureMessage(testPayload, secured);
    if (!result.is_success()) {
        Serial.println("ERROR: Failed to secure message");
        return false;
    }
    
    String envelope = security->generateSecuredEnvelope(secured);
    Serial.println("✓ Message secured");
    
    Serial.println("[6.3] Sending to server...");
    Serial.print("  URL: ");
    Serial.println(TEST_SERVER_URL);
    Serial.println("  Envelope:");
    Serial.println(envelope);
    
    HTTPClient http;
    http.begin(TEST_SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Device-ID", DEVICE_ID);
    
    int httpCode = http.POST(envelope);
    String response = http.getString();
    http.end();
    
    Serial.print("[6.4] Response code: ");
    Serial.println(httpCode);
    Serial.println("[6.5] Response body:");
    Serial.println(response);
    
    if (httpCode != 200) {
        Serial.println("ERROR: Server returned non-200 status!");
        return false;
    }
    
    Serial.println("✓ Server communication successful!");
    return true;
}

/**
 * TEST 7: Replay Attack Detection
 */
bool test_replay_attack() {
    printTestHeader("TEST 7: Replay Attack Detection");
    
    Serial.println("[7.1] Creating message...");
    const char* plain = "{\"test\":\"replay\"}";
    
    SecuredMessage secured;
    security->secureMessage(plain, secured);
    String envelope = security->generateSecuredEnvelope(secured);
    
    Serial.println("[7.2] First verification (should succeed)...");
    String recovered1;
    SecurityResult result1 = security->verifyMessage(envelope.c_str(), recovered1);
    if (!result1.is_success()) {
        Serial.println("ERROR: First verification failed!");
        return false;
    }
    Serial.println("✓ First verification passed");
    
    Serial.println("[7.3] Second verification (should fail - replay)...");
    String recovered2;
    SecurityResult result2 = security->verifyMessage(envelope.c_str(), recovered2);
    if (result2.is_success()) {
        Serial.println("ERROR: Replay attack not detected!");
        return false;
    }
    
    Serial.print("✓ Replay detected! Status: ");
    Serial.println((int)result2.status);
    
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n");
    Serial.println("╔═══════════════════════════════════════════════════════╗");
    Serial.println("║  Milestone 4 Part 3 - Security Layer Test Program   ║");
    Serial.println("║  ESP32 Security Implementation Verification          ║");
    Serial.println("╚═══════════════════════════════════════════════════════╝");
    Serial.println();
    
    // Connect to WiFi
    Serial.println("Connecting to WiFi...");
    WiFi.begin("PCD", "12345678");  // Use your WiFi credentials
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("✓ WiFi Connected! IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("✗ WiFi Connection Failed!");
        Serial.println("Note: Some tests will be skipped.");
    }
    
    Serial.println("\n" + String("=").substring(0, 56));
    Serial.println("Starting Security Tests");
    Serial.println(String("=").substring(0, 56) + "\n");
    
    // Run all tests
    printTestResult("Initialization", test_initialization());
    printTestResult("HMAC Computation", test_hmac());
    printTestResult("Nonce Generation", test_nonce());
    printTestResult("Secure Message Creation", test_secure_message());
    printTestResult("Round-trip Verification", test_roundtrip());
    
    if (WiFi.status() == WL_CONNECTED) {
        printTestResult("Server Communication", test_server_communication());
    } else {
        Serial.println("⊘ SKIP: Server Communication (WiFi not connected)");
    }
    
    printTestResult("Replay Attack Detection", test_replay_attack());
    
    // Print summary
    Serial.println("\n" + String("=").substring(0, 56));
    Serial.println("TEST SUMMARY");
    Serial.println(String("=").substring(0, 56));
    Serial.print("Total Tests: ");
    Serial.println(tests_passed + tests_failed);
    Serial.print("Passed: ");
    Serial.println(tests_passed);
    Serial.print("Failed: ");
    Serial.println(tests_failed);
    
    if (tests_failed == 0) {
        Serial.println("\n🎉 ALL TESTS PASSED! 🎉");
        Serial.println("Security implementation is working correctly!");
    } else {
        Serial.println("\n⚠️  SOME TESTS FAILED ⚠️");
        Serial.println("Please review the test output above.");
    }
    Serial.println(String("=").substring(0, 56) + "\n");
}

void loop() {
    // Nothing to do in loop
    delay(1000);
}
