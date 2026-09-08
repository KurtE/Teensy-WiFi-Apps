#include <Arduino_RouterBridge.h>

#define ARDUINOJSON_USE_LONG_LONG 0
#include <ArduinoJson.h>

// UDP Configuration
BridgeUDP<4096> udp(Bridge);
const unsigned int localPort = 8888;       // Port Arduino listens on
const char* remoteIPStr      = "192.168.1.12"; // Remote target IP
const unsigned int remotePort = 3333;       // Remote target Port

void setup() {
  Serial.begin(115200);
  
  udp.begin(localPort);
  char msg[128];
  snprintf(msg, sizeof(msg), "Listening on UDP port %d", localPort);
  Serial.println(msg);
}

void loop() {
  // 1. RECEIVE & DESERIALIZE GENERIC MSGPACK PACKET
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    // Read the binary packet into a local buffer
    uint8_t packetBuffer[packetSize];
    udp.read(packetBuffer, packetSize);

    // Create a dynamic document to parse arbitrary msgpack payloads
    JsonDocument doc;
    DeserializationError error = deserializeMsgPack(doc, packetBuffer, packetSize);

    if (!error) {
      Serial.println("Received MsgPack message successfully!");
      
      // Accessing generic data safely (checks if key exists, otherwise defaults)
      if (doc.containsKey("sensor")) {
        const char* sensorName = doc["sensor"];
        float val = doc["value"];
        char msg[128];
        snprintf(msg, sizeof(msg), "Sensor: %s, Value: %.2f", sensorName, val);
        Serial.println(msg);
      } else {
        // Fallback: Print raw structure converted to JSON format for debugging
        serializeJson(doc, Serial); 
        Serial.println();
      }
    } else {
      Serial.print("MsgPack deserialization failed: ");
      Serial.println(error.c_str());
    }
  }

  // 2. SERIALIZE & SEND GENERIC MSGPACK PACKET (Every 5 seconds)
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 5000) {
    lastSend = millis();

    // Construct a generic data map
    JsonDocument doc;
    doc["device"] = "Arduino_Node_1";
    doc["uptime"] = millis() / 1000;
    
    // Nested Array example
    JsonArray data = doc["data"].to<JsonArray>();
    data.add(23.5); // temperature
    data.add(60);   // humidity

    // Measure binary layout footprint size 
    size_t len = measureMsgPack(doc);
    uint8_t outputBuffer[len];

    // Export payload object directly to our byte array buffer
    serializeMsgPack(doc, outputBuffer, len);

    // Ship the binary packet out over UDP
    udp.beginPacket(remoteIPStr, remotePort);
    udp.write(outputBuffer, len);
    udp.endPacket();

    Serial.println("Generic MsgPack UDP packet sent.");
  }
}