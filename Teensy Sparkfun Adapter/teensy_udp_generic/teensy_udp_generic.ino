// SPDX-FileCopyrightText: (c) 2021-2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <cerrno>
#include <cstring>
#include <ctime>
#include <ArduinoJson.h>
#include <QNEthernet.h>
#include "MsgPackUDP.h"

using namespace qindesign::network;

// Configuration rules
constexpr uint32_t kDHCPTimeout = 15000;
constexpr uint16_t kNTPPort = 3333;
constexpr uint16_t remotePort = 8888;
const char* udpAddressStr = "192.168.1.205";

// Instantiate class wrapper
MsgPackUDP reliableNode(kNTPPort);

// Hardware Initialization Safety Guard
bool networkReady = false;

void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) {}
  printf("Starting Network Bridge Node...\r\n");

  if (!Ethernet.begin()) {
    printf("CRITICAL ERROR: Failed to start Ethernet hardware link!\r\n");
    while (1) { delay(1000); } // Lock safely here to prevent loop() crash
  }
  
  printf("Waiting for local IP via DHCP...\r\n");
  if (!Ethernet.waitForLocalIP(kDHCPTimeout)) {
    printf("CRITICAL ERROR: DHCP Timeout! No IP assigned.\r\n");
    while (1) { delay(1000); } // Lock safely here to prevent loop() crash
  }

  // Initialize your dynamic reliable processing block engine
  if (reliableNode.begin()) {
    networkReady = true;
    printf("Reliable socket active and listening on port %d.\r\n", kNTPPort);
  } else {
    printf("CRITICAL ERROR: Failed to bind UDP socket!\r\n");
    while (1) { delay(1000); }
  }
}

void loop1() {    //Use with python udp_test app
  // Never run socket tasks if initialization failed
  if (!networkReady) return;

  // 1. RECEIVE & PROCESS RELIABLE ENVELOPES FROM PYTHON
  JsonDocument genericIncoming;
  if (reliableNode.receiveReliable(genericIncoming)) {
    Serial.println("Received verified MsgPack message successfully!");
    
    if (genericIncoming.containsKey("temp")) {
      float temperature = genericIncoming["temp"];
      Serial.printf("Extracted Telemetry Data -> Temp: %.2f\n", temperature);
    } else {
      serializeJson(genericIncoming, Serial); 
      Serial.println();
    }
  }

  // 2. DISPATCH WRAPPED DATA TO LINUX EVERY 5 SECONDS
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 5000) {
    lastSend = millis();

    IPAddress targetIP;
    if (targetIP.fromString(udpAddressStr)) {
      JsonDocument doc;
      doc["device"] = "Teensy_41_Node";
      doc["uptime"] = millis() / 1000;
      
      Serial.println("Dispatching dynamic data map safely to Python...");
      bool deliveryOk = reliableNode.sendReliable(targetIP, remotePort, doc);
      
      if (deliveryOk) {
        Serial.println("  >> Confirmation ACK logged successfully from Linux.");
      } else {
        Serial.println("  >> Error: Frame transmission timed out.");
      }
    }
  }
}

void loop() {   //use with weather Udp app
  // Never run socket tasks if initialization failed
  if (!networkReady) return;

  // 1. RECEIVE & PROCESS RELIABLE ENVELOPES FROM PYTHON
  JsonDocument genericIncoming;
  if (reliableNode.receiveReliable(genericIncoming)) {
    
    // Check if the payload is our weather tracking dictionary
    if (genericIncoming.containsKey("msg_type") && 
        strcmp(genericIncoming["msg_type"], "WEATHER_DATA") == 0) {
      
      Serial.println("\n==========================================");
      Serial.println("[TEENSY RECV] Live Weather Synchronized!");
      
      // FIX: Updated keys to match the shorter naming convention from Python
      float outdoorTemp = genericIncoming["temp"] | 0.0f;
      int   humidity    = genericIncoming["humid"] | 0;
      int   weatherCode = genericIncoming["wcode"] | 0;
      
      Serial.printf("  Outdoor Temp : %.2f °C\n", outdoorTemp);
      Serial.printf("  Humidity     : %d %%\n", humidity);
      Serial.printf("  WMO Sky Code : %d\n", weatherCode);
      Serial.println("==========================================");
      
    } else {
      // Fallback display handling for any other miscellaneous packets sent by Python
      Serial.print("[TEENSY RECV] Unhandled envelope signature: ");
      serializeJson(genericIncoming, Serial); 
      Serial.println();
    }
  }

  // 2. DISPATCH WRAPPED DATA TO LINUX EVERY 5 SECONDS (Keep your existing uptime sender code here...)
}
