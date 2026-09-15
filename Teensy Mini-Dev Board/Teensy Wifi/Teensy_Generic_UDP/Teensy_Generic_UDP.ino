#include <QNEthernet.h>
#include "MsgPackUDP.h" // Ensure the Teensy QNEthernet version of this file is in your folder

using namespace qindesign::network;

// Network Rules: Listen on 4444, Target ESP32 on 3333
const uint16_t localPort = 3333;
const uint16_t esp32Port = 8888;
const char* esp32IPStr   = "192.168.1.221"; // Update to your ESP32's IP

MsgPackUDP reliableNode(localPort);
bool networkReady = false;

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
  while (!Serial && (millis() < 4000)) {}
  printf("Starting Teensy Node...\r\n");

  if (!Ethernet.begin()) {
    printf("Failed to start Ethernet hardware.\r\n");
    while (1) delay(1000);
  }
  
  if (!Ethernet.waitForLocalIP(15000)) {
    printf("DHCP Timeout failed.\r\n");
    while (1) delay(1000);
  }

  if (reliableNode.begin()) {
    networkReady = true;
    IPAddress localIP = Ethernet.localIP();
    printf("Teensy Online. IP: %u.%u.%u.%u\n", localIP[0], localIP[1], localIP[2], localIP[3]);
  }
}

void loop() {
  if (!networkReady) return;

  // ==========================================
  // PHASE 1: RECEIVE DATA FROM ESP32
  // ==========================================
  JsonDocument incomingDoc;
  if (reliableNode.receiveReliable(incomingDoc)) {
    Serial.println("\n[TEENSY RECV] Incoming packet from ESP32 recognized!");
    
    const char* msgType = incomingDoc["msg_type"] | "UNKNOWN";
    
    if (strcmp(msgType, "STRUCT_DATA") == 0) {
      DeviceStatus remoteStatus = incomingDoc["payload"].as<DeviceStatus>();
      Serial.printf("  > Type: Struct\n  > Node ID: %d, Battery: %.2fV, Active: %s\n", 
                    remoteStatus.node_id, remoteStatus.battery, remoteStatus.is_active ? "YES" : "NO");
    }
    else if (strcmp(msgType, "ARRAY_DATA") == 0) {
      JsonArrayConst arr = incomingDoc["payload"];
      Serial.printf("  > Type: Array (%d elements)\n  > Values: ", arr.size());
      for (JsonVariantConst v : arr) Serial.printf("%d ", v.as<int>());
      Serial.println();
    }
    else if (strcmp(msgType, "GENERIC_DATA") == 0) {
      Serial.printf("  > Type: Generic Primitive\n  > Value: %s\n", incomingDoc["payload"].as<const char*>());
    }
  }

  // ==========================================
  // PHASE 2: TRANSMIT CORRESPONDING DESIGNS BACK
  // ==========================================
  static unsigned long lastTx = 0;
  static int currentStep = 0;
  
  if (millis() - lastTx > 4000) { // Send different data types every 4 seconds (offset from ESP32)
    lastTx = millis();
    
    IPAddress targetIP;
    if (!targetIP.fromString(esp32IPStr)) return;

    JsonDocument doc;

    switch (currentStep) {
      case 0: { // SENDING STRUCT DATA
        Serial.println("\n[TEENSY TX] Distributing state struct to ESP32...");
        doc["msg_type"] = "STRUCT_DATA";
        
        DeviceStatus localStatus = {999, 3.82f, false};
        doc["payload"] = localStatus;
        
        bool ok = reliableNode.sendReliable(targetIP, esp32Port, doc);
        Serial.println(ok ? "  >> ESP32 confirmed structural receipt." : "  >> Timeout error.");
        break;
      }
      case 1: { // SENDING ARRAY DATA
        Serial.println("\n[TEENSY TX] Distributing a float Array to ESP32...");
        doc["msg_type"] = "ARRAY_DATA";
        
        JsonArray arr = doc["payload"].to<JsonArray>();
        arr.add(12.3f);
        arr.add(45.6f);
        arr.add(78.9f);
        
        bool ok = reliableNode.sendReliable(targetIP, esp32Port, doc);
        Serial.println(ok ? "  >> ESP32 confirmed array receipt." : "  >> Timeout error.");
        break;
      }
      case 2: { // SENDING GENERIC PRIMITIVE DATA
        Serial.println("\n[TEENSY TX] Distributing basic string diagnostic to ESP32...");
        doc["msg_type"] = "GENERIC_DATA";
        doc["payload"]  = "TEENSY_CORE_CRITICAL_OK";
        
        bool ok = reliableNode.sendReliable(targetIP, esp32Port, doc);
        Serial.println(ok ? "  >> ESP32 confirmed generic receipt." : "  >> Timeout error.");
        break;
      }
    }
    
    currentStep = (currentStep + 1) % 3;
  }
}
