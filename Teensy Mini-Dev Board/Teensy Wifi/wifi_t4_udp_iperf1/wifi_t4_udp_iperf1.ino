#include <SPI.h>
#include <QNEthernet.h>

using namespace qindesign::network;
EthernetUDP _udp; 

// iperf -u -c <Teensy IP> -p 5001 -b 1M
//  change datagram size
// iperf -u -c 192.168.1.12 -l 2048 -p 5001 -b 1M

// Network settings (adjust for your network)
unsigned int localPort = 5001;       // iperf default UDP port

EthernetUDP Udp;

// Stats
unsigned long packetCount = 0;
unsigned long lastSeq = 0;
unsigned long lostPackets = 0;
unsigned long startTime = 0;
unsigned long totalBytes = 0;

void setup();
void loop();
void setup() {
  Serial.begin(115200);

  // Start Ethernet
  if (!Ethernet.begin()) {
    printf("Failed to start Ethernet hardware.\r\n");
    while (1) delay(1000);
  }
  
  uint8_t mac[6];
  Ethernet.macAddress(mac);  // This is informative; it retrieves, not sets
  printf("MAC = %02x:%02x:%02x:%02x:%02x:%02x\r\n",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);


  if (!Ethernet.waitForLocalIP(15000)) {
    printf("DHCP Timeout failed.\r\n");
    while (1) delay(1000);
  }

  IPAddress ip = Ethernet.localIP();
  printf("    Local IP    = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  ip = Ethernet.subnetMask();
  printf("    Subnet mask = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  ip = Ethernet.gatewayIP();
  printf("    Gateway     = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  ip = Ethernet.dnsServerIP();
  printf("    DNS         = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);

  // Start UDP
  if (!Udp.begin(localPort)) {
    Serial.println("UDP start failed");
    while (true);
  }

  Serial.println("UDP Throughput Test Ready");
}

void loop() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    uint8_t buffer[2000]; // 1500 for typical MTU
    int len = Udp.read(buffer, sizeof(buffer));

    if (len >= 12) { 
      // iperf UDP header format:
      // 0-3: sequence number (signed int, big-endian)
      // 4-7: timestamp seconds
      // 8-11: timestamp microseconds

      int32_t seqNum = 
        ((int32_t)buffer[0] << 24) |
        ((int32_t)buffer[1] << 16) |
        ((int32_t)buffer[2] << 8)  |
        ((int32_t)buffer[3]);

      if (packetCount == 0) {
        // First packet
        lastSeq = seqNum;
      } else {
        if (seqNum > lastSeq) {
          lostPackets += (uint32_t)(seqNum - lastSeq - 1);
        } 
        else if (seqNum < lastSeq) {
          // Sequence reset or wrap-around — start fresh
          lostPackets = 0;
          packetCount = 0;
        }
      }

      lastSeq = seqNum;
      packetCount++;
      totalBytes += len;
    }
  }

  // Print stats every 2 seconds
  if (millis() - startTime >= 2000) {
    float seconds = (millis() - startTime) / 1000.0f;
    float kbps = (totalBytes * 8.0f) / 1000.0f / seconds;
    float lossPercent = (packetCount + lostPackets) > 0 ?
                        (lostPackets * 100.0f) / (packetCount + lostPackets) : 0;

    Serial.print("Packets: ");
    Serial.print(packetCount);
    Serial.print(" | Lost: ");
    Serial.print(lostPackets);
    Serial.print(" (");
    Serial.print(lossPercent, 2);
    Serial.print("%) | Throughput: ");
    Serial.print(kbps, 2);
    Serial.println(" kbps");

    // Reset counters for next interval
    packetCount = 0;
    lostPackets = 0;
    totalBytes = 0;
    startTime = millis();
  }
}

