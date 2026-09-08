#include <WiFi.h>
#include "MsgPackUDP.h" // Ensure the ESP32 version of this file is in your tab/folder

// Wi-Fi Configuration
#include "Secrets.h"

// Network Rules: Listen on 3333, Target Teensy on 4444
const uint16_t localPort  = 8888;
const uint16_t teensyPort = 3333;
const char* teensyIPStr   = "192.168.1.12"; // Update to your Teensy's IP

MsgPackUDP reliableNode(localPort);

// A custom data structure to demonstrate structured data transfer
struct DeviceStatus {
  int node_id;
  float battery;
  bool is_active;
};

// MANDATORY: Tells ArduinoJson how to pack the struct into MessagePack
void convertToJson(const DeviceStatus& src, JsonVariant dst) {
  dst["id"]   = src.node_id;
  dst["bat"]  = src.battery;
  dst["act"]  = src.is_active;
}

// MANDATORY: Tells ArduinoJson how to unpack MessagePack back into the struct
void convertFromJson(JsonVariantConst src, DeviceStatus& dst) {
  dst.node_id  = src["id"] | 0;
  dst.battery  = src["bat"] | 0.0f;
  dst.is_active = src["act"] | false;
}

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nESP32 Connected. IP: %s\n", WiFi.localIP().toString().c_str());

  reliableNode.begin();
}

void loop() {
  // ==========================================
  // PHASE 1: RECEIVE DATA FROM TEENSY
  // ==========================================
  JsonDocument incomingDoc;
  if (reliableNode.receiveReliable(incomingDoc)) {
    Serial.println("\n[ESP32 RECV] Incoming packet from Teensy recognized!");
    
    const char* msgType = incomingDoc["msg_type"] | "UNKNOWN";
    
    // Check if it's the structure type
    if (strcmp(msgType, "STRUCT_DATA") == 0) {
      DeviceStatus remoteStatus = incomingDoc["payload"].as<DeviceStatus>();
      Serial.printf("  > Type: Struct\n  > Node ID: %d, Battery: %.2fV, Active: %s\n", 
                    remoteStatus.node_id, remoteStatus.battery, remoteStatus.is_active ? "YES" : "NO");
    }
    // Check if it's the raw array type
    else if (strcmp(msgType, "ARRAY_DATA") == 0) {
      JsonArrayConst arr = incomingDoc["payload"];
      Serial.printf("  > Type: Array (%d elements)\n  > Values: ", arr.size());
      for (JsonVariantConst v : arr) Serial.printf("%.1f ", v.as<float>());
      Serial.println();
    }
    // Check if it's an unstructured fallback primitive string
    else if (strcmp(msgType, "GENERIC_DATA") == 0) {
      Serial.printf("  > Type: Generic Primitive\n  > Value: %s\n", incomingDoc["payload"].as<const char*>());
    }
  }

  // ==========================================
  // PHASE 2: SEQUENTIAL TRANSMISSION TO TEENSY
  // ==========================================
  static unsigned long lastTx = 0;
  static int currentStep = 0;
  
  if (millis() - lastTx > 4000) { // Cycle through different data modes every 4 seconds
    lastTx = millis();
    
    IPAddress targetIP;
    if (!targetIP.fromString(teensyIPStr)) return;

    JsonDocument doc;

    switch (currentStep) {
      case 0: { // TYPE 1: STRUCTURE
        Serial.println("\n[ESP32 TX] Sending a strict Struct definition...");
        doc["msg_type"] = "STRUCT_DATA";
        
        DeviceStatus localStatus = {101, 4.15f, true};
        doc["payload"] = localStatus; // Uses custom convertToJson handler automatically
        
        bool ok = reliableNode.sendReliable(targetIP, teensyPort, doc);
        Serial.println(ok ? "  >> Teensy confirmed structural receipt." : "  >> Timeout error.");
        break;
      }
      case 1: { // TYPE 2: ARRAY
        Serial.println("\n[ESP32 TX] Sending an Array of control positions...");
        doc["msg_type"] = "ARRAY_DATA";
        
        JsonArray arr = doc["payload"].to<JsonArray>();
        arr.add(45);  // Servo 1
        arr.add(90);  // Servo 2
        arr.add(180); // Servo 3
        
        bool ok = reliableNode.sendReliable(targetIP, teensyPort, doc);
        Serial.println(ok ? "  >> Teensy confirmed array receipt." : "  >> Timeout error.");
        break;
      }
      case 2: { // TYPE 3: UNSTRUCTURED GENERIC PRIMITIVE
        Serial.println("\n[ESP32 TX] Sending a generic unmapped string frame...");
        doc["msg_type"] = "GENERIC_DATA";
        doc["payload"]  = "ESP32_SYSTEM_ALIVE"; // Plain string primitive target
        
        bool ok = reliableNode.sendReliable(targetIP, teensyPort, doc);
        Serial.println(ok ? "  >> Teensy confirmed generic frame receipt." : "  >> Timeout error.");
        break;
      }
    }
    
    currentStep = (currentStep + 1) % 3; // Shift loop execution step indexes
  }
}
