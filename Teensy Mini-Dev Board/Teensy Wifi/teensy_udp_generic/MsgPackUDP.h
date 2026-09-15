#pragma once
#include <ArduinoJson.h>
#include <QNEthernet.h>

using namespace qindesign::network; 

enum PacketType { MSG = 1, ACK = 2 };

class MsgPackUDP {
private:
  EthernetUDP _udp; 
  unsigned int _localPort;
  uint32_t _sequenceCounter = 0;

  uint32_t _timeoutMs = 150; 
  uint8_t  _maxRetries = 4;

  // Hardened static sizing protection to prevent stack smashing crashes
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
    if (len > MAX_PACKET_SIZE) {
      Serial.println("[ACK-ERR] Payload too large for local buffers!");
      return false;
    }
    
    serializeMsgPack(envelopeDoc, _ioBuffer, len);

    for (uint8_t attempt = 0; attempt <= _maxRetries; attempt++) {
      if (attempt > 0) {
        Serial.printf("[ACK-LOG] Retry %d/%d for Msg ID: %u\n", attempt, _maxRetries, currentSeq);
      }

      if (!sendRawBuffer(remoteIP, remotePort, _ioBuffer, len)) {
        delay(5);
        continue; 
      }

      unsigned long startWait = millis();
      while (millis() - startWait < _timeoutMs) {
        int ackPacketSize = _udp.parsePacket();
        if (ackPacketSize > 0 && ackPacketSize <= (int)MAX_PACKET_SIZE) {
          
          // Read directly into memory boundary safely
          _udp.read(_ioBuffer, ackPacketSize);

          JsonDocument incomingAck;
          DeserializationError err = deserializeMsgPack(incomingAck, _ioBuffer, ackPacketSize);
          
          if (!err && 
              incomingAck["_type"].as<int>() == static_cast<int>(PacketType::ACK) && 
              incomingAck["_id"].as<uint32_t>() == currentSeq) {
            Serial.printf("[ACK-LOG] Confirmed! Match verified for ID: %u\n", currentSeq);
            return true; 
          }
        }
        yield(); 
      }
    }

    Serial.printf("[ACK-WARN] Message ID %u dropped. Host failed to ACK.\n", currentSeq);
    return false; 
  }

  template <typename T>
  bool sendValueReliable(const IPAddress& remoteIP, uint16_t remotePort, const T& value) {
    JsonDocument doc;
    doc.set(value);
    return sendReliable(remoteIP, remotePort, doc);
  }

  template <typename T>
  bool sendStructReliable(const IPAddress& remoteIP, uint16_t remotePort, const T& customStruct) {
    JsonDocument doc;
    doc.set(customStruct);
    return sendReliable(remoteIP, remotePort, doc);
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

  template <typename T>
  bool receiveStructReliable(T& outStruct) {
    JsonDocument doc;
    if (!receiveReliable(doc)) return false;
    outStruct = doc.as<T>();
    return true;
  }
};
