// SPDX-FileCopyrightText: (c) 2021-2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// SNTPClient demonstrates a simple SNTP client.
// See: https://tools.ietf.org/html/rfc4330
//
// This file is part of the QNEthernet library.

// C includes
#include <sys/time.h>
// Assume settimeofday() exists

// C++ includes
#include <cerrno>
#include <cstring>
#include <ctime>

#include <QNEthernet.h>

using namespace qindesign::network;

// --------------------------------------------------------------------------
//  Configuration
// --------------------------------------------------------------------------

constexpr uint32_t kDHCPTimeout = 15000;  // 15 seconds

constexpr uint16_t kNTPPort = 3333;
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

   // Send the packet
  printf("Sending SNTP request to the gateway...");
 

   udp.beginPacket(udpAddress, kNTPPort);
   udp.write(packetBuffer, 48);
   udp.endPacket();
   printf("\r\n");
   printf("Started listening\r\n");
}

// Main program loop.
void loop() {
  // 1. RECEIVE DATA
  // Check if a packet has arrived
  int packetSize = udp.parsePacket();
 
  if (packetSize > 0) {
    Serial.printf("Received %d bytes: \n", packetSize);
  // Read the packet into packetBuffer
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0; // Null-terminate the string
    }
    Serial.printf("Packet contents: %s\n", packetBuffer);
  }

 // 2. SEND DATA (Every 5 seconds)
  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime > 5000) {
    lastSendTime = millis();

    Serial.println("Sending periodic UDP packet...");
   
    // Initialize packet transmission to target IP and port
    udp.beginPacket(udpAddress, kNTPPort);
   
    // Print formatted data directly into the packet
    udp.printf("Hello from Teensy 4.1! Uptime: %lu seconds", millis() / 1000);
   
    // Finalize and transmit the packet
    udp.endPacket();
  }
}