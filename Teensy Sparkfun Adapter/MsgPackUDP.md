# `MsgPackUDP`

Supports UDP Generic Teensy Sketches.

A lightweight, high-performance, reliable UDP network transport library designed for embedded microcontrollers (such as Teensy 4.1). Built on top of **[QNEthernet](https://github.com/ssilverman/QNEthernet)** and **[ArduinoJson v7](https://arduinojson.org/)**, `MsgPackUDP` provides TCP-like reliability guarantees (automatic retransmission, sequencing, and acknowledgement frames) over raw UDP using compact binary **MessagePack** serialization.

---

## Key Features

* **Guaranteed Delivery over UDP:** Implements explicit sequence tracking, configurable ACK confirmation, and automatic timeout-based retransmissions.
* **Compact Binary Payload:** Uses MessagePack binary formatting via `ArduinoJson` to minimize packet size, bandwidth overhead, and parsing CPU cycles.
* **Static Memory Safety:** Hardened buffer boundary protections capped at standard Ethernet MTU size (`1500` bytes) to prevent stack smashing and buffer overflow vulnerabilities.
* **Flexible Schemas:** Direct support for `JsonDocument` objects, primitive types, nested arrays, and custom C++ structures.
* **Zero External Allocation Overhead:** Works seamlessly with standard C++ stack and global memory allocations.

---

## Dependencies

* **[QNEthernet](https://github.com/ssilverman/QNEthernet)** (v0.17.0 or higher recommended)
* **[ArduinoJson](https://arduinojson.org/)** (v7.0.0 or higher required)

---

## Architecture & Wire Protocol

Every message sent using `sendReliable()` is wrapped inside an internal framing envelope before MessagePack binary serialization:

```json
{
  "_type": 1,         // 1 = MSG, 2 = ACK
  "_id": 1042,        // Auto-incrementing sequence counter
  "payload": { ... }  // User payload (JSON object, array, structure, or primitive)
}

```

When an endpoint receives a packet with `"_type": 1`, it immediately generates and replies with an explicit acknowledgement frame directed to the sender's origin IP and port:

```json
{
  "_type": 2,         // ACK frame
  "_id": 1042         // Matching sequence counter ID
}

```

---

## API Reference

### Class Constructor & Network Initialization

#### `MsgPackUDP(unsigned int port)`

Instantiates the wrapper class and assigns the local listening UDP socket port.

```cpp
MsgPackUDP node(3333); // Listens on local port 3333

```

#### `bool begin()`

Binds the underlying UDP socket to the assigned local port. Returns `true` on successful binding.

```cpp
if (!node.begin()) {
    Serial.println("Failed to bind UDP socket!");
}

```

#### `void setReliabilityConfig(uint32_t timeoutMs, uint8_t maxRetries)`

Configures transmission reliability parameters.

* **`timeoutMs`** *(default: `150`)*: Timeout duration in milliseconds to wait for a matching ACK before retransmitting.
* **`maxRetries`** *(default: `4`)*: Maximum number of retransmission attempts before declaring link failure.

```cpp
node.setReliabilityConfig(200, 3); // 200ms timeout, up to 3 retries

```

---

### Sending Data

#### `bool sendReliable(const IPAddress& remoteIP, uint16_t remotePort, const JsonDocument& payloadDoc)`

Encapsulates and sends a `JsonDocument` payload to the target IP and port. Blocks momentarily while awaiting acknowledgment or timing out through retries.

* **Returns:** `true` if an explicit matching ACK is received within the timeout window; `false` on retry failure or memory overflow.

#### `template <typename T> bool sendValueReliable(const IPAddress& remoteIP, uint16_t remotePort, const T& value)`

Helper function to serialize and send a single primitive data type (e.g., `int`, `float`, `const char*`).

#### `template <typename T> bool sendStructReliable(const IPAddress& remoteIP, uint16_t remotePort, const T& customStruct)`

Helper function to serialize and send a custom C++ structure using custom ArduinoJson `convertToJson()` conversion hooks.

---

### Receiving Data

#### `bool receiveReliable(JsonDocument& outPayloadDoc)`

Non-blocking frame check for incoming packets. When a valid `MSG` frame arrives, `receiveReliable()` automatically sends an ACK frame to the sender, unswizzles the inner payload into `outPayloadDoc`, and returns `true`.

#### `template <typename T> bool receiveStructReliable(T& outStruct)`

Non-blocking frame check that automatically deserializes incoming payloads directly into a target C++ structure via custom ArduinoJson `convertFromJson()` conversion hooks.

---

## Practical Examples

### Example 1: Basic Node Communication

```cpp
#include <ArduinoJson.h>
#include <QNEthernet.h>
#include "MsgPackUDP.h"

using namespace qindesign::network;

MsgPackUDP node(3333);
IPAddress remoteHost(192, 168, 1, 205);
uint16_t remotePort = 8888;

void setup() {
  Serial.begin(115200);
  Ethernet.begin();
  Ethernet.waitForLocalIP(10000);

  if (node.begin()) {
    Serial.println("MsgPackUDP Node Initialized!");
  }
}

void loop() {
  // 1. Non-blocking receiver check
  JsonDocument incoming;
  if (node.receiveReliable(incoming)) {
    Serial.println("Reliable packet received!");
    serializeJson(incoming, Serial);
    Serial.println();
  }

  // 2. Periodic periodic transmission
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 5000) {
    lastSend = millis();

    JsonDocument doc;
    doc["device"] = "Teensy_Node";
    doc["uptime"] = millis() / 1000;

    if (node.sendReliable(remoteHost, remotePort, doc)) {
      Serial.println("Delivery confirmed by host!");
    } else {
      Serial.println("Delivery failed after maximum retries.");
    }
  }
}

```

---

### Example 2: Sending and Receiving Custom Structures

ArduinoJson v7 supports direct custom C++ struct conversion through `convertToJson` and `convertFromJson` mapping functions.

#### 1. Define Struct and Mapping Hooks

```cpp
#include <ArduinoJson.h>

// Define target structure
struct TelemetryFrame {
  uint32_t timestamp;
  float temperature;
  float humidity;
  bool systemOk;
};

// Map C++ Structure -> JsonVariant (Serialization)
inline void convertToJson(const TelemetryFrame& src, JsonVariant dst) {
  dst["ts"]   = src.timestamp;
  dst["temp"] = src.temperature;
  dst["hum"]  = src.humidity;
  dst["ok"]   = src.systemOk;
}

// Map JsonVariantConst -> C++ Structure (Deserialization)
inline void convertFromJson(JsonVariantConst src, TelemetryFrame& dst) {
  dst.timestamp   = src["ts"] | 0UL;
  dst.temperature = src["temp"] | 0.0f;
  dst.humidity    = src["hum"] | 0.0f;
  dst.systemOk    = src["ok"] | false;
}

```

#### 2. Transmitting a Struct

```cpp
TelemetryFrame frame = {
  .timestamp   = millis(),
  .temperature = 24.3f,
  .humidity    = 52.1f,
  .systemOk    = true
};

IPAddress targetIP(192, 168, 1, 205);

if (node.sendStructReliable(targetIP, 8888, frame)) {
  Serial.println("Telemetry frame structure confirmed by remote host!");
}

```

#### 3. Receiving a Struct

```cpp
TelemetryFrame incomingFrame;

if (node.receiveStructReliable(incomingFrame)) {
  Serial.printf("[STRUCT RECV] TS: %u | Temp: %.2f C | Hum: %.2f %% | OK: %s\n",
                incomingFrame.timestamp,
                incomingFrame.temperature,
                incomingFrame.humidity,
                incomingFrame.systemOk ? "TRUE" : "FALSE");
}

```

---

### Example 3: Working with Arrays

#### 1. Transmitting Numerical and Heterogeneous Arrays

```cpp
JsonDocument doc;

// Option A: Constructing arrays dynamically via JsonArray
JsonArray telemetryList = doc["readings"].to<JsonArray>();
telemetryList.add(101.3);
telemetryList.add(102.5);
telemetryList.add(99.8);

// Option B: Assigning raw C++ fixed arrays or vectors directly
float sensorBuffer[] = { 1.05f, 2.33f, 0.89f, 4.12f };
doc["buffer"] = sensorBuffer;

IPAddress targetIP(192, 168, 1, 205);

if (node.sendReliable(targetIP, 8888, doc)) {
  Serial.println("Array payload successfully sent and acknowledged!");
}

```

#### 2. Receiving and Parsing Arrays

```cpp
JsonDocument doc;

if (node.receiveReliable(doc)) {
  // Option A: Iterating through an array via JsonArray
  if (doc.containsKey("readings")) {
    JsonArray readings = doc["readings"];
    Serial.printf("Received array with %u items:\n", readings.size());
    
    for (float val : readings) {
      Serial.printf(" - Value: %.2f\n", val);
    }
  }

  // Option B: Copying directly from JsonVariant into a C++ fixed array
  if (doc.containsKey("buffer")) {
    float localArray[4];
    copyArray(doc["buffer"], localArray);
    
    Serial.printf("Copied Raw Buffer Element [0]: %.2f\n", localArray[0]);
  }
}

```

---

## License

This project is released under the **MIT License**.