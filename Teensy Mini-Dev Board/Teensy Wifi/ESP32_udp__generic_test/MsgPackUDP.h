#pragma once

#include <ArduinoJson.h>

#if defined(ARDUINO_TEENSY41)
#include <QNEthernet.h>
using namespace qindesign::network; 
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFiUdp.h>
#endif

enum PacketType { MSG = 1, ACK = 2 };

class MsgPackUDP {
private:
#if defined(ARDUINO_TEENSY41)
  EthernetUDP _udp; 
#else
  WiFiUDP _udp; // Using ESP32 Core WiFi UDP engine
#endif
  unsigned int _localPort;
  uint32_t _sequenceCounter = 0;

  uint32_t _timeoutMs = 150;
  uint8_t  _maxRetries = 4;

  // Static buffer to handle large arrays safely without crashing the ESP32 stack
  static const size_t MAX_PACKET_SIZE = 1500; 
  uint8_t _ioBuffer[MAX_PACKET_SIZE]; 

  bool sendRawBuffer(const IPAddress& remoteIP, uint16_t remotePort, const uint8_t* buffer, size_t len) {
    if (_udp.beginPacket(remoteIP, remotePort) != 1) return false;
    if (_udp.write(buffer, len) != len) return false;
    return _udp.endPacket() == 1;
  }

  void sendAck(const IPAddress& remoteIP, uint16_t remotePort, uint32_t targetSeqId) {
    JsonDocument ackDoc;
    ackDoc["_type"] = static_cast<int>(PacketType::ACK);
    ackDoc["_id"]   = targetSeqId;

    size_t len = measureMsgPack(ackDoc);
    if (len > MAX_PACKET_SIZE) return;

    serializeMsgPack(ackDoc, _ioBuffer, len);
    sendRawBuffer(remoteIP, remotePort, _ioBuffer, len);
  }

public:
  MsgPackUDP(unsigned int port) : _localPort(port) {}

  bool begin() {
    return _udp.begin(_localPort) == 1;
  }

  void setReliabilityConfig(uint32_t timeoutMs, uint8_t maxRetries) {
    _timeoutMs = timeoutMs;
    _maxRetries = maxRetries;
  }

  bool sendReliable(const IPAddress& remoteIP, uint16_t remotePort, const JsonDocument& payloadDoc) {
    _sequenceCounter++;
    uint32_t currentSeq = _sequenceCounter;

    JsonDocument envelopeDoc;
    envelopeDoc["_type"] = static_cast<int>(PacketType::MSG);
    envelopeDoc["_id"]   = currentSeq;
    envelopeDoc["payload"] = payloadDoc;

    size_t len = measureMsgPack(envelopeDoc);
    if (len > MAX_PACKET_SIZE) return false;
    
    serializeMsgPack(envelopeDoc, _ioBuffer, len);

    for (uint8_t attempt = 0; attempt <= _maxRetries; attempt++) {
      if (!sendRawBuffer(remoteIP, remotePort, _ioBuffer, len)) {
        delay(5);
        continue; 
      }

      unsigned long startWait = millis();
      while (millis() - startWait < _timeoutMs) {
        int ackPacketSize = _udp.parsePacket();
        if (ackPacketSize > 0 && ackPacketSize <= (int)MAX_PACKET_SIZE) {
          _udp.read(_ioBuffer, ackPacketSize);

          JsonDocument incomingAck;
          DeserializationError err = deserializeMsgPack(incomingAck, _ioBuffer, ackPacketSize);
          
          if (!err && 
              incomingAck["_type"].as<int>() == static_cast<int>(PacketType::ACK) && 
              incomingAck["_id"].as<uint32_t>() == currentSeq) {
            return true; 
          }
        }
        yield(); 
      }
    }
    return false; 
  }

  bool receiveReliable(JsonDocument& outPayloadDoc) {
    int packetSize = _udp.parsePacket();
    if (packetSize <= 0 || packetSize > (int)MAX_PACKET_SIZE) return false;

    _udp.read(_ioBuffer, packetSize);

    JsonDocument envelopeDoc;
    DeserializationError error = deserializeMsgPack(envelopeDoc, _ioBuffer, packetSize);
    if (error != DeserializationError::Ok) return false;

    int type = envelopeDoc["_type"] | 0;
    uint32_t msgId = envelopeDoc["_id"] | 0;

    if (type == static_cast<int>(PacketType::MSG)) {
      sendAck(_udp.remoteIP(), _udp.remotePort(), msgId);
      outPayloadDoc.set(envelopeDoc["payload"]);
      return true;
    }
    return false; 
  }
};
