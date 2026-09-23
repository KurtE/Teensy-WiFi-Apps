#define USE_KEYBOARD
//#define printForecast
#define orientation 0  // or 1 (landscape)

#include <SPI.h>
#include "RA8876_Config_SPI.h"
#include <RA8876_t3.h>
 // Common optimized Teensy driver
#include <PNGdec.h>      // PNG decoder library by bitbank2

#include "font_Arial.h"
#include "font_ArialBold.h"

#include <XPT2046_RA8876.h>
#define CS_PIN  7
//XPT2046 ts(CS_PIN);
#define TIRQ_PIN  2
XPT2046 ts(CS_PIN,TIRQ_PIN);  // Param 2 - Touch IRQ Pin - interrupt enabled polling
//XPT2046 ts(CS_PIN);  // Param 2 - Touch IRQ Pin - interrupt enabled polling

// XPT2046 typical raw ADC limits (adjust if edges are slightly off)
#define TS_MINX 1921
#define TS_MAXX 171
#define TS_MINY 1974
#define TS_MAXY 70

/**************************************************
* Setup Teensy Wifi                               *
***************************************************/
#include <QNEthernet.h>
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include "forwardDecs_RA8876.h"

// Allocated document capacity
JsonDocument doc;

using namespace qindesign::network;

const char *server = "api.open-meteo.com";
const char *geocoding_api_server = "geocoding-api.open-meteo.com";
const char *air_quality_server = "air-quality-api.open-meteo.com";

const int port = 80;
constexpr uint32_t kDHCPTimeout = 15000;

EthernetClient client;

#include "location_RA8876.h"
#ifndef DEFAULT_CITY
#define DEFAULT_CITY "Los Angeles"
#endif

String weather_city = DEFAULT_CITY;
String weather_time_zone = "";
double weather_latitude = 0;
double weather_longitude = 0;


/**************************************************
* Define Display pins and sd card Chipselect pin  *
***************************************************/
// Pin assignments (adjust to your hardware setup)
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8
#define SD_CS     BUILTIN_SDCARD     // Adjust to your SD CS pin (if using SD card)

RA8876_t3 tft = RA8876_t3(RA8876_CS, RA8876_RESET); //Using standard SPI pins

#if defined(USE_KEYBOARD)
#include <RA8876_t4_Keypad.h>
// easy way to include fonts but change globally
#define FONT_BUTTON Arial_16  // font for keypad buttons
//uint16_t ScreenLeft = 30, ScreenRight = 468, ScreenTop = 302, ScreenBottom = 3;

Keyboard MyKeyboard(&tft, &ts);
#endif //use keyboard

/**************************************************
* Initialize PNG Libraray                         *
***************************************************/
PNG png;

// Display Offsets for PNG callback position
int16_t pngX = 0;
int16_t pngY = 0;

/************************************************/

uint32_t g_weather_cycle_time_ms = (uint32_t)(60 * 60 * 1000); // cycle time in MS 10 * 60 *1000;
uint32_t g_last_cycle_time_ms = 0;

/*************************************************/
void setup() {

  Serial.println("initialization done.");
#if defined(USE_SPI_47000000)
  tft.begin(47000000); // Max is 47000000 MHz (using short 3" wires)
#else
  tft.begin(); // default SPI clock speed is 30000000 MHz 
#endif
  tft.setRotation(0); // Landscape (480x320)
  tft.setOrigin(0,0);
  tft.fillScreen(COLOR_BG);
  tft.setFont(Arial_10);

  Serial.println("initialization done.");

  if (!Ethernet.begin()) {
    Serial.println("Failed to configure Ethernet using DHCP");
    return;
  }

  Serial.printf("Waiting for local IP...\r\n");
  if (!Ethernet.waitForLocalIP(kDHCPTimeout)) {
    Serial.printf("Failed to get IP address from DHCP\r\n");
    return;
  }

  Serial.print("Local IP: ");
  Serial.println(Ethernet.localIP());

  appState = (weather_time_zone.length() == 0)? FETCH_MAP_CITY_TO_LOCATION : FETCH_CURRENT;
  ts.begin(1024,600);
  // Replace these for your screen module
  ts.setCalibration(1921, 1974, 171, 70);

/*
  if (orientation == 3) {
    ScreenLeft = 267; ScreenRight = 3855; ScreenTop = 167; ScreenBottom = 3855;
  } else if (orientation == 1) {
    ScreenLeft = 3855; ScreenRight = 267; ScreenTop = 3855; ScreenBottom = 167;
  }
*/
#if defined(USE_KEYBOARD)
  MyKeyboard.init(COLOR_BLACK, COLOR_WHITE, COLOR_BLUE, COLOR_DARKGREY, COLOR_DARKGREY, COLOR_NAVY, COLOR_BLACK, FONT_BUTTON);
//  MyKeyboard.setTouchLimits( ScreenLeft, ScreenRight, ScreenTop, ScreenBottom);
  // optional methods
  // max input characters is controlled by in the .h file
  // #define MAX_KEYBOARD_CHARS 18
  // change input display color
  MyKeyboard.setDisplayColor(COLOR_WHITE, COLOR_BLUE);
  // want rounded corners?
  // MyKeyboard.setCornerRadius(3);
  // Set initial instructions
  // MyKeyboard.setInitialText("IP 111.222.333.444");
  // MyKeyboard.hideInput(); // for hidden password input
  // optional to populate the input box
  //  strcpy(MyKeyboard.data, "TEXT");
#endif //use keyboard
}

void loop() {
  switch (appState) {
    case FETCH_MAP_CITY_TO_LOCATION:
      Serial.println("\n[0/3] Map City to Location...");
      if (sendMapCityRequest()) {
        appState = READ_MAP_CITY_TO_LOCATION;
      } else {
        Serial.println("Connection failed.");
        appState = DONE_APP;
      }
      break;
      
    case READ_MAP_CITY_TO_LOCATION:
      if (processIncomingStream(printMapCityData)) {
        appState = FETCH_CURRENT;
      }
      break;

    case FETCH_CURRENT:
      Serial.println("\n[1/3] Requesting Current Weather...");
      if (sendCurrentRequest()) {
        appState = READ_CURRENT;
      } else {
        Serial.println("Connection failed.");
        appState = DONE_APP;
      }
      break;

    case READ_CURRENT:
      if (processIncomingStream(printCurrentData)) {
        appState = FETCH_DAILY;  //was FETCH_HOURLY
      }
      break;

    case FETCH_HOURLY:
      Serial.println("\n[2/3] Requesting Hourly Forecast...");
      if (sendHourlyRequest()) {
        appState = READ_HOURLY;
      } else {
        Serial.println("Connection failed.");
        appState = DONE_APP;
      }
      break;

    case READ_HOURLY:
      if (processIncomingStream(printHourlyData)) {
        appState = FETCH_DAILY;
      }
      break;

    case FETCH_DAILY:
      Serial.println("\n[3/3] Requesting Daily Forecast...");
      if (sendDailyRequest()) {
        appState = READ_DAILY;
      } else {
        Serial.println("Connection failed.");
        appState = DONE_APP;
      }
      break;

    case READ_DAILY:
      if (processIncomingStream(printDailyData)) {
        appState = FETCH_AIR_QUALITY;
      }
      break;
    
    case FETCH_AIR_QUALITY:
      Serial.println("\n[4/4] Requesting Air Quality...");
      if (sendAirQualityRequest()) {
        appState = READ_AIR_QUALITY;
      } else {
        Serial.println("Connection failed.");
        appState = DONE_APP;
      }
      break;

    case READ_AIR_QUALITY:
      if (processIncomingStream(printAirQualityData)) {
        drawWeatherDashboard();
        Serial.println("All data successfully fetched!");
        Serial.println("Enter City name:");
        appState = DONE_APP;
      }
      break;

    case DONE_APP:
      break;
    default:
      break;
  }

  uint16_t touchX = 0;
  uint16_t touchY = 0;

  if (ts.isTouching()) {

    ts.getPosition(touchX, touchY);
    touchX = constrain(touchX, 0, 1024);
    touchY = constrain(touchY, 0, 600);

    // STATE 1: Processing touches on the Main Dashboard
    if (currentScreen == SCREEN_MAIN) {
      int selectedDay = getTouchedForecastCard(touchX, touchY);
      if (selectedDay != -1) {
        //Serial.printf("Opening Detail View for Day %d\n", selectedDay);
        showDayDetailScreen(selectedDay);
        delay(300); // Debounce touch
      }
#if defined(USE_KEYBOARD)
      if(isKeyboardClicked(touchX, touchY)) {
        showKeyboard();
      }
#endif
    } 
    // STATE 2: Processing touches on the Detail Screen
    else if (currentScreen == SCREEN_DETAIL) {
      if (isBackButtonClicked(touchX, touchY)) {
        //Serial.println("Back Button Pressed! Returning to Main Dashboard...");
        showMainDashboard();
        delay(300); // Debounce touch
      }
    }
  }
  #if defined(USE_KEYBOARD)
    else if (currentScreen == SCREEN_KEYBOARD) {
      if (isBackButtonClicked(touchX, touchY)) {
        //Serial.println("Back Button Pressed! Returning to Main Dashboard...");
        showMainDashboard();
        delay(50); // Debounce touch
      }
    }
#endif //use keyboard

  if (Serial.available()) {
    weather_city = Serial.readString();
    weather_city.trim();
    weather_city.replace(' ', '+');
    Serial.print("New City: ");
    Serial.println(weather_city);
    appState = FETCH_MAP_CITY_TO_LOCATION; 
  }

  // see if we timed out and should start a new read cycle
  uint32_t delta_time = millis() - g_last_cycle_time_ms;
  if (delta_time > g_weather_cycle_time_ms) {
    Serial.println("\n*** Start new read cycle ***");
    appState = (weather_time_zone.length() == 0)? FETCH_MAP_CITY_TO_LOCATION : FETCH_CURRENT;
    g_last_cycle_time_ms = millis(); // don't keep hitting this 
  }
}

// Returns card index (0 to 4) if pressed, or -1 if touch is outside cards
int getTouchedForecastCard(int touchX, int touchY) {
  int colWidth = 85;
  int startX = 40;

  int cardY = 185;
  int cardW = colWidth - 8; // 77px wide
  int cardH = 128;         // 128px tall

  // Loop through all 5 cards and check bounding boxes
  for (int i = 0; i < 5; i++) {
    int cardX = (startX + (i * colWidth)) - 5;

    if (touchX >= cardX && touchX <= (cardX + cardW) &&
        touchY >= cardY && touchY <= (cardY + cardH)) {
      return i; // Touched card index
    }
  }

  return -1; // No forecast card touched
}

bool isBackButtonClicked(int touchX, int touchY) {
  // Back button bounds matching: drawRoundRect(20, 260, 100, 40)
  int btnX = 20;
  int btnY = 260;
  int btnW = 100;
  int btnH = 40;

  return (touchX >= btnX && touchX <= (btnX + btnW) &&
          touchY >= btnY && touchY <= (btnY + btnH));
}


#if defined(USE_KEYBOARD)
bool isKeyboardClicked(int touchX, int touchY) {
  //Serial.printf("%d, %d\n", touchX, touchY);

  int btnX = 400;
  int btnY = 0;
  int btnW = 44;
  int btnH = 14;

  return (touchX >= btnX && touchX <= (btnX + btnW) &&
          touchY >= btnY && touchY <= (btnY + btnH));
}
#endif
