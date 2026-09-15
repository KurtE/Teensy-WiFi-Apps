#include <Arduino_RouterBridge.h>
#include <ArduinoJson.h>

// Network configuration

// UDP Configuration
BridgeUDP<4096> udp(Bridge);
// UDP settings
const unsigned int localPort = 3333;      // Port the ESP32 listens on
const char* udpAddress = "192.168.1.12"; // Target IP address to send data to
const int remotePort = 3333;              // Target port to send data to
// Packet buffers
char packetBuffer[255]; 
char replyBuffer[] = "Message received!";

void setup() {
  Serial.begin(115200);
  
  udp.begin(localPort);
  //printf("Listening on UDP port %d\n", localPort);
}

void loop() {
    // Send a packet
    udp.beginPacket(udpAddress, remotePort);
    udp.write((const uint8_t*)"PING", 4);
    udp.endPacket();

    // Try to receive response
    unsigned long startTime = millis();
    int packetSize = 0;

    while (millis() - startTime < 5000) {
        packetSize = udp.parsePacket();
        if (packetSize > 0) {
            char buffer[64];
            int bytesRead = udp.read(buffer, min(packetSize, 63));
            buffer[bytesRead] = '\0';

            Monitor.print("Received response: ");
            Monitor.println(buffer);

            break;
        }
        delay(10);
    }

    delay(100);
}

