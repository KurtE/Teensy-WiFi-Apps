// SPDX-FileCopyrightText: (c) 2021-2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// SNTPClient demonstrates a simple SNTP client.
// See: https://tools.ietf.org/html/rfc4330
//
// This file is part of the QNEthernet library.

// C++ includes
#include <cerrno>
#include <cstring>
#include <ctime>
#include <ArduinoJson.h>
#include <QNEthernet.h>

using namespace qindesign::network;

// --------------------------------------------------------------------------
//  Configuration
// --------------------------------------------------------------------------

constexpr uint32_t kDHCPTimeout = 15000;  // 15 seconds

constexpr uint16_t kNTPPort = 3333;
constexpr uint16_t remotePort = 8888;
// --------------------------------------------------------------------------
//  Program State
// --------------------------------------------------------------------------

namespace {  // Internal linkage section

// UDP port.
EthernetUDP udp;

// Buffer.
uint8_t packetBuffer[254];

}  // namespace

// --------------------------------------------------------------------------
//  Main Program
// --------------------------------------------------------------------------

const char* udpAddress = "192.168.1.205";

// Program setup.
void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) {
    // Wait for Serial
  }
  printf("Starting...\r\n");

  printf("Starting Ethernet with DHCP...\r\n");
  if (!Ethernet.begin()) {
    printf("Failed to start Ethernet\r\n");
    return;
  }
  uint8_t mac[6];
  Ethernet.macAddress(mac);  // This is informative; it retrieves, not sets
  printf("MAC = %02x:%02x:%02x:%02x:%02x:%02x\r\n",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  printf("Waiting for local IP...\r\n");
  if (!Ethernet.waitForLocalIP(kDHCPTimeout)) {
    printf("Failed to get IP address from DHCP\r\n");
    return;
  }

  IPAddress ip = Ethernet.localIP();
  printf("    Local IP    = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  ip = Ethernet.subnetMask();
  printf("    Subnet mask = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  ip = Ethernet.gatewayIP();
  printf("    Gateway     = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  ip = Ethernet.dnsServerIP();
  printf("    DNS         = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);

  // Start UDP listening on the NTP port
  udp.begin(kNTPPort);

  // Send an SNTP request

  std::memset(packetBuffer, 0, 254);

}

// Main program loop.
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
        Serial.printf("Sensor: %s, Value: %.2f\n", sensorName, val);
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
    udp.beginPacket(udpAddress, remotePort);
    udp.write(outputBuffer, len);
    udp.endPacket();

    Serial.println("Generic MsgPack UDP packet sent.");
  }
}
