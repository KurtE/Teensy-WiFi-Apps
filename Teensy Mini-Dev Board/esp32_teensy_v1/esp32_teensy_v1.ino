#include "Arduino.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "Secrets.h"

#define USING_PROTOSUPLY_C6_STACK_SERIAL1

// -------------------------------------------------------------------
// Wi-Fi Credentials & Hardware Pins
// -------------------------------------------------------------------
const char* ssid = SSID;
const char* password = PASSWORD;

#ifdef USING_PROTOSUPLY_C6_STACK_SERIAL1
#define RX1 2  // Teensy 4.1 Serial 1 is connected to serial port #1
#define TX1 1
#else
#define RX2 20  // Connect to Teensy 4.1 TX
#define TX2 21  // Connect to Teensy 4.1 RX
#endif

#define RESET_PIN D3

WiFiClient client;
JsonDocument doc;

// -------------------------------------------------------------------
// API Endpoint Configuration
// -------------------------------------------------------------------
const char* server = "api.open-meteo.com";
const char* geocoding_api_server = "geocoding-api.open-meteo.com";
const char* air_quality_server = "air-quality-api.open-meteo.com";
const int port = 80;

// Dynamic Location Parameters (Updated dynamically via Geocoding)
String weather_city = "New York";
float weather_latitude = 40.7128;
float weather_longitude = -74.0060;
String weather_time_zone = "America/New_York";

// -------------------------------------------------------------------
// State Machine Definitions
// -------------------------------------------------------------------
enum AppState {
  IDLE,
  FETCH_CITY,
  FETCH_CURRENT,
  FETCH_HOURLY,
  FETCH_DAILY,
  FETCH_AIR_QUALITY
};

AppState currentState = IDLE;

// Forward Declarations
bool sendMapCityRequest();
bool sendCurrentRequest();
bool sendHourlyRequest();
bool sendDailyRequest();
bool sendAirQualityRequest();
bool processIncomingStream(const char* typeLabel);

// -------------------------------------------------------------------
// Helper to Construct Exact URL Query Strings
// -------------------------------------------------------------------
String buildQueryString(const char* basePath, JsonDocument& params) {
  String query = String(basePath);
  bool first = true;

  JsonObject obj = params.as<JsonObject>();
  for (JsonPair kv : obj) {
    query += first ? '?' : '&';
    first = false;

    query += kv.key().c_str();
    query += '=';

    if (kv.value().is<JsonArray>()) {
      bool firstElement = true;
      for (JsonVariant val : kv.value().as<JsonArray>()) {
        if (!firstElement) query += ',';
        query += val.as<String>();
        firstElement = false;
      }
    } else {
      query += kv.value().as<String>();
    }
  }
  return query;
}

// -------------------------------------------------------------------
// Request Builders
// -------------------------------------------------------------------
bool sendMapCityRequest() {
  client.stop();
  if (!client.connect(geocoding_api_server, port)) return false;

  JsonDocument params;
  params["name"] = weather_city;
  params["count"] = 1;

  String resource = buildQueryString("/v1/search", params);
  Serial.print("Geocoding Query: ");
  Serial.println(resource);

  client.print("GET ");
  client.print(resource.c_str());
  client.print(" HTTP/1.0\r\nHost: geocoding-api.open-meteo.com\r\nUser-Agent: ESP32-C3\r\nConnection: close\r\n\r\n");
  return true;
}

bool sendCurrentRequest() {
  client.stop();
  if (!client.connect(server, port)) return false;

  JsonDocument params;
  params["latitude"] = weather_latitude;
  params["longitude"] = weather_longitude;

  JsonArray current = params["current"].to<JsonArray>();
  current.add("temperature_2m");
  current.add("wind_speed_10m");
  current.add("wind_direction_10m");
  current.add("weather_code");
  current.add("surface_pressure");
  current.add("rain");
  current.add("snowfall");
  current.add("precipitation");

  params["timezone"] = weather_time_zone;
  params["wind_speed_unit"] = "mph";
  params["temperature_unit"] = "fahrenheit";
  params["precipitation_unit"] = "inch";

  String resource = buildQueryString("/v1/forecast", params);

  client.print("GET ");
  client.print(resource.c_str());
  client.print(" HTTP/1.0\r\nHost: api.open-meteo.com\r\nUser-Agent: ESP32-C3\r\nConnection: close\r\n\r\n");
  return true;
}

bool sendHourlyRequest() {
  client.stop();
  if (!client.connect(server, port)) return false;

  JsonDocument params;
  params["latitude"] = weather_latitude;
  params["longitude"] = weather_longitude;

  JsonArray hourly = params["hourly"].to<JsonArray>();
  hourly.add("temperature_2m");
  hourly.add("precipitation_probability");
  hourly.add("precipitation");
  hourly.add("rain");
  hourly.add("snowfall");
  hourly.add("pressure_msl");
  hourly.add("wind_speed_10m");

  params["timezone"] = weather_time_zone;
  params["forecast_days"] = 1;
  params["wind_speed_unit"] = "mph";
  params["temperature_unit"] = "fahrenheit";
  params["precipitation_unit"] = "inch";

  String resource = buildQueryString("/v1/forecast", params);

  client.print("GET ");
  client.print(resource.c_str());
  client.print(" HTTP/1.0\r\nHost: api.open-meteo.com\r\nUser-Agent: ESP32-C3\r\nConnection: close\r\n\r\n");
  return true;
}

bool sendDailyRequest() {
  client.stop();
  if (!client.connect(server, port)) return false;

  JsonDocument params;
  params["latitude"] = weather_latitude;
  params["longitude"] = weather_longitude;

  JsonArray daily = params["daily"].to<JsonArray>();
  daily.add("temperature_2m_max");
  daily.add("temperature_2m_min");
  daily.add("snowfall_sum");
  daily.add("precipitation_probability_max");
  daily.add("weather_code");
  daily.add("wind_speed_10m_max");
  daily.add("wind_gusts_10m_max");
  daily.add("wind_direction_10m_dominant");
  daily.add("rain_sum");
  daily.add("precipitation_sum");
  daily.add("sunrise");
  daily.add("sunset");
  daily.add("relative_humidity_2m_mean");
  daily.add("cloud_cover_mean");

  params["timezone"] = weather_time_zone;
  params["wind_speed_unit"] = "mph";
  params["temperature_unit"] = "fahrenheit";
  params["precipitation_unit"] = "inch";

  String resource = buildQueryString("/v1/forecast", params);

  client.print("GET ");
  client.print(resource.c_str());
  client.print(" HTTP/1.0\r\nHost: api.open-meteo.com\r\nUser-Agent: ESP32-C3\r\nConnection: close\r\n\r\n");
  return true;
}

bool sendAirQualityRequest() {
  client.stop();
  if (!client.connect(air_quality_server, port)) return false;

  JsonDocument params;
  params["latitude"] = weather_latitude;
  params["longitude"] = weather_longitude;

  JsonArray current = params["current"].to<JsonArray>();
  current.add("us_aqi");
  current.add("pm2_5");
  current.add("pm10");

  params["timezone"] = weather_time_zone;

  String resource = buildQueryString("/v1/air-quality", params);

  client.print("GET ");
  client.print(resource.c_str());
  client.print(" HTTP/1.0\r\nHost: air-quality-api.open-meteo.com\r\nUser-Agent: ESP32-C3\r\nConnection: close\r\n\r\n");
  return true;
}

// -------------------------------------------------------------------
// Stream Processor & Data Emitter
// -------------------------------------------------------------------
bool processIncomingStream(const char* typeLabel) {
  if (!client.connected() && client.available() == 0) {
    return true;
  }

  // Wait briefly for network buffer to fill
  uint32_t timeout = millis();
  while (client.available() == 0 && (millis() - timeout < 2000)) {
    delay(10);
  }

  // Find end of HTTP headers (\r\n\r\n)
  if (client.find("\r\n\r\n")) {
    String payload = "";
    payload.reserve(3072);  // Pre-allocate heap to avoid fragmentation

    uint32_t start = millis();
    while ((client.connected() || client.available()) && (millis() - start < 3000)) {
      while (client.available()) {
        payload += (char)client.read();
      }
    }

    // Extract dynamic coordinates internally if this was a geocoding lookup
    if (strcmp(typeLabel, "CITY") == 0) {
      doc.clear();
      DeserializationError err = deserializeJson(doc, payload);
      if (!err && doc["results"][0].containsKey("latitude")) {
        weather_latitude = doc["results"][0]["latitude"].as<float>();
        weather_longitude = doc["results"][0]["longitude"].as<float>();

        if (doc["results"][0].containsKey("timezone")) {
          weather_time_zone = doc["results"][0]["timezone"].as<String>();
        }

        Serial.printf("[ESP32 Geo Update] Lat: %.4f, Lon: %.4f, TZ: %s\n",
                      weather_latitude, weather_longitude, weather_time_zone.c_str());
      } else {
        Serial.println("[ESP32 Geo Error] Failed to extract coordinates from payload!");
      }
    }

    // Send formatted raw response to Teensy 4.1 over Serial1
    Serial1.print("JSON:");
    Serial1.print(typeLabel);
    Serial1.print(":");
    Serial1.println(payload);

    client.stop();
    return true;
  }

  client.stop();
  return false;
}

// -------------------------------------------------------------------
// Arduino Setup
// -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);  // USB Serial Monitor

// Modify to C6 stack
// Disconnect unused I/O to avoid possible conflicts
#ifdef USING_PROTOSUPLY_C6_STACK_SERIAL1
  gpio_reset_pin(GPIO_NUM_0);
  // gpio_reset_pin(GPIO_NUM_1);  // Used if connecting by Serial1
  // gpio_reset_pin(GPIO_NUM_2);  // Used if connecting by Serial1
  gpio_reset_pin(GPIO_NUM_16);
  gpio_reset_pin(GPIO_NUM_17);  // Used if connecting by Serial5
  gpio_reset_pin(GPIO_NUM_18);
  gpio_reset_pin(GPIO_NUM_19);  // Used if connecting by Serial5
  gpio_reset_pin(GPIO_NUM_20);
//  gpio_reset_pin(GPIO_NUM_21);
  gpio_reset_pin(GPIO_NUM_22);
  gpio_reset_pin(GPIO_NUM_23);

  Serial1.begin(4000000, SERIAL_8N1, RX1, TX1);  // Hardware UART to Teensy 4.1
#else
  Serial1.begin(115200, SERIAL_8N1, RX2, TX2);  // Hardware UART to Teensy 4.1
#endif

  // Tr to setup a reset pin... Pin 3 here PIN 2 on Arduino.
#ifdef RESET_PIN  
  pinMode(RESET_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), reset_esp32, FALLING);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);

  #endif

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Perform handshake with Teensy 4.1
  while (true) {
    if (Serial1.available()) {
      char cmd = Serial1.read();
      if (cmd == '?') {
        Serial1.print("Y\n");
        Serial.println("Teensy Handshake Complete!");
        break;
      }
    }
  }
}

// -------------------------------------------------------------------
// Maybe reboot ESP32
// -------------------------------------------------------------------
void reset_esp32() {
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  esp_restart();
}

// -------------------------------------------------------------------
// Arduino Loop (State Machine Execution)
// -------------------------------------------------------------------
void loop() {
  // 1. Process Incoming Commands from Teensy 4.1
  if (Serial1.available()) {
    String commandLine = Serial1.readStringUntil('\n');
    commandLine.trim();

    if (commandLine.startsWith("CMD:CITY:")) {
      weather_city = commandLine.substring(9);
      weather_city.replace(' ', '+');  // URL safety
      if (sendMapCityRequest()) {
        currentState = FETCH_CITY;
      } else {
        Serial1.println("ERR:CONN_FAILED");
      }
    } else if (commandLine == "CMD:CURRENT") {
      if (sendCurrentRequest()) currentState = FETCH_CURRENT;
      else Serial1.println("ERR:CONN_FAILED");
    } else if (commandLine == "CMD:HOURLY") {
      if (sendHourlyRequest()) currentState = FETCH_HOURLY;
      else Serial1.println("ERR:CONN_FAILED");
    } else if (commandLine == "CMD:DAILY") {
      if (sendDailyRequest()) currentState = FETCH_DAILY;
      else Serial1.println("ERR:CONN_FAILED");
    } else if (commandLine == "CMD:AQI") {
      if (sendAirQualityRequest()) currentState = FETCH_AIR_QUALITY;
      else Serial1.println("ERR:CONN_FAILED");
    }
  }

  // 2. State-Based Stream Processing
  switch (currentState) {
    case FETCH_CITY:
      if (processIncomingStream("CITY")) currentState = IDLE;
      break;

    case FETCH_CURRENT:
      if (processIncomingStream("CURRENT")) currentState = IDLE;
      break;

    case FETCH_HOURLY:
      if (processIncomingStream("HOURLY")) currentState = IDLE;
      break;

    case FETCH_DAILY:
      if (processIncomingStream("DAILY")) currentState = IDLE;
      break;

    case FETCH_AIR_QUALITY:
      if (processIncomingStream("AQI")) currentState = IDLE;
      break;

    case IDLE:
    default:
      break;
  }
}