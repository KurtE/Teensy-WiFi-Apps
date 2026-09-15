/**********************************************
*  Setup display and touch
***********************************************/
//#define ILI9488_DISP // else ST7796
#define XPT_TOUCH  //else FT6236
#define USE_KEYBOARD
//#define printForecast
#define orientation 1  // or 1 (landscape)

String DEFAULT_CITY = "Disneyland"; // Default startup city
/*********************************************
*  Configure ArduinoJson Library
***********************************************/
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
// Allocated document capacity
JsonDocument doc;

/**************************************************/

#include <SPI.h>

#if defined(ILI9488_DISP)
#include "ILI9488_t3.h"
#else
#include <ST7796_t3.h> // Hardware-specific library
#endif

// Pin assignments (adjust to your hardware setup)
#define TFT_CS   7
#define TFT_DC    9
#define TFT_RST   8

#define ESP32_RESET_PIN 2

#if defined(ILI9488_DISP)
ILI9488_t3 tft = ILI9488_t3(&SPI, TFT_CS, TFT_DC, TFT_RST);
#else
ST7796_t3 tft = ST7796_t3(TFT_CS, TFT_DC, TFT_RST);
#endif

 // Common optimized Teensy driver
#include <PNGdec.h>      // PNG decoder library by bitbank2

#include "font_Arial.h"
#include "font_ArialBold.h"

#if defined(XPT_TOUCH)
#include <XPT2046_Touchscreen.h>
#define TOUCH_CS 6
#define TOUCH_SPI SPI
XPT2046_Touchscreen ts(TOUCH_CS);  // Param 2 - Touch IRQ Pin - interrupt enabled polling

// XPT2046 typical raw ADC limits (adjust if edges are slightly off)
// from my ST7796 X(223, 3893), Y(340, 3815)
#define TS_MINX 200
#define TS_MAXX 3800
#define TS_MINY 200
#define TS_MAXY 3800

#else
#include <Adafruit_FT6206.h>
Adafruit_FT6206 ts = Adafruit_FT6206();
#endif
#if defined(USE_KEYBOARD)
#include <ILI9341_t3_Keypad.h>
// easy way to include fonts but change globally
#define FONT_BUTTON Arial_12  // font for keypad buttons

// Orientation=1
//uint16_t ScreenLeft = 3, ScreenRight = 478, ScreenTop = 302, ScreenBottom = 2;
// Orientation = 3
//int16_t ScreenLeft = 471, ScreenRight = 4, ScreenTop = 9, ScreenBottom = 319;
uint16_t ScreenLeft = 3893, ScreenRight = 223, ScreenTop = 3815, ScreenBottom = 340;

Keyboard MyKeyboard(&tft, &ts);
//[[maybe_unused]] uint16_t ScreenLeft = 30, ScreenRight = 468, ScreenTop = 302, ScreenBottom = 3;
#endif

/*****************************************************
*  Config weather app / ESP32 Connection
******************************************************/
#include "forwardDecs.h"
#include "weatherApp.h"

#define ESP32SERIAL Serial5  //7 for C3, 5 for C5/C6
#define ESP32SERIAL_BUFFER_SIZE 4 * 1024
unsigned char esp32SerialBuffer[ESP32SERIAL_BUFFER_SIZE];

bool esp32Attached = false;
String aweather_city = "Disneyland";

//Auto refresh every hour
uint32_t g_weather_cycle_time_ms = (uint32_t)(60 * 60 * 1000); // cycle time in MS 10 * 60 *1000;
uint32_t g_last_cycle_time_ms = 0;

/***********************************************************/

void setup() {
#ifdef ESP32_RESET_PIN
  pinMode(ESP32_RESET_PIN, INPUT_PULLUP);
#endif

  Serial.begin(115200);
  ESP32SERIAL.begin(4000000);
  ESP32SERIAL.addMemoryForRead(esp32SerialBuffer, ESP32SERIAL_BUFFER_SIZE);

  // Clear startup garbage from line
  while (ESP32SERIAL.available()) {
    ESP32SERIAL.read();
  }

  // Handshake loop
  uint8_t ping_count = 0;
  while (!esp32Attached) {
    #ifdef ESP32_RESET_PIN
    ping_count++;
    if (ping_count == 10) {
      Serial.println("Reset ESP32");
      pinMode(ESP32_RESET_PIN, OUTPUT);
      digitalWrite(ESP32_RESET_PIN, LOW);
      delay(5);
      pinMode(ESP32_RESET_PIN, INPUT_PULLUP);
      delay(3000);
      ping_count = 0;
    }
    #endif
    Serial.println("Ping ESP32-C3...");
    ESP32SERIAL.print("?");
    
    uint32_t start = millis();
    while (millis() - start < 500) {
      if (ESP32SERIAL.available()) {
        String res = ESP32SERIAL.readStringUntil('\n');
        res.trim();
        if (res == "Y") {
          esp32Attached = true;
          Serial.println("ESP32-C3 Connected!");
          break;
        }
      }
    }
  }

#if defined(ILI9488_DISP)
  tft.begin();
#else
  tft.init(320, 480);
//  tft.invertDisplay(true);  //black display
#endif
  tft.setRotation(orientation); // Landscape (480x320)
  tft.setOrigin(0,0);
  tft.fillScreen(COLOR_BG);
  tft.setFont(Arial_10);

  Serial.println("initialization done.");

#if defined(XPT_TOUCH)
  ts.begin(TOUCH_SPI);
  if (orientation == 3) {
    ScreenLeft = 267; ScreenRight = 3869; 
    ScreenTop = 167; ScreenBottom = 3863;
  } else if (orientation == 1) {
    ScreenLeft = 3869; ScreenRight = 267; 
    ScreenTop = 3863; ScreenBottom = 167;
  }
#else
  ts.begin(40, &Wire);
  if(orientation == 1) {
    ScreenLeft = 3; ScreenRight = 478;
    ScreenTop = 302; ScreenBottom = 2;
  } else if(orientation == 3) {
    ScreenLeft = 471;  creenRight = 4; 
    ScreenTop = 9; ScreenBottom = 319;
  }
#endif

#if defined(USE_KEYBOARD)
  MyKeyboard.init(COLOR_BLACK, COLOR_WHITE, COLOR_BLUE, COLOR_DARKGREY, COLOR_DARKGREY, COLOR_NAVY, COLOR_BLACK, FONT_BUTTON);
  MyKeyboard.setTouchLimits( ScreenLeft, ScreenRight, ScreenTop, ScreenBottom);
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
  //MyKeyboard.setInitialText("New City");
  // optional to populate the input box
  //  strcpy(MyKeyboard.data, "TEXT");
#endif //use keyboard

  delay(500);

  // 3. Trigger Startup Weather Request
  Serial.print("Booting into default city: ");
  Serial.println(DEFAULT_CITY);
  
  ESP32SERIAL.print("CMD:CITY:");
  ESP32SERIAL.println(DEFAULT_CITY);

  Serial.println("INPUT NEW CITY.....");

}

void loop() {
  // 1. Send City Request when user enters text into Serial Monitor
  if (Serial.available()) {
    weather_city = Serial.readStringUntil('\n');
    weather_city.trim();
    
    if (weather_city.length() > 0) {
      Serial.print("Requesting City: ");
      Serial.println(weather_city);

      // Send execution commands across UART
      ESP32SERIAL.print("CMD:CITY:");
      ESP32SERIAL.println(weather_city);
    }
  }

  // see if we timed out and should start a new read cycle
  uint32_t delta_time = millis() - g_last_cycle_time_ms;
  if (delta_time > g_weather_cycle_time_ms) {
    Serial.println("\n*** Start new read cycle ***");
    if(weather_time_zone.length() == 0) {
      ESP32SERIAL.print("CMD:CITY:");
      ESP32SERIAL.println(weather_city);
    } else {
      Serial.println(weather.location);
      ESP32SERIAL.print("CMD:CITY:");
      ESP32SERIAL.println(weather.location);
    }
    g_last_cycle_time_ms = millis(); // don't keep hitting this 
  }

  // 2. Non-blocking Async Reception of Responses from ESP32-C3
  if (ESP32SERIAL.available()) {
    String response = ESP32SERIAL.readStringUntil('\n');
    response.trim();

    if (response.startsWith("JSON:CITY:")) {
      Serial.println("\n--- [GEOCODING DATA RECEIVED] ---");
      Serial.println(response.substring(10));

      processIncomingStream(printMapCityData, response.substring(10));
      
      // Chain next request: CURRENT
      ESP32SERIAL.println("CMD:CURRENT");
    } 
    else if (response.startsWith("JSON:CURRENT:")) {
      Serial.println("\n--- [CURRENT WEATHER RECEIVED] ---");
      Serial.println(response.substring(13));

      processIncomingStream(printCurrentData, response.substring(13));

      // Chain next request: HOURLY
      ESP32SERIAL.println("CMD:DAILY");
    } 
    else if (response.startsWith("JSON:HOURLY:")) {
      Serial.println("\n--- [HOURLY FORECAST RECEIVED] ---");
      Serial.println(response.substring(12));

      // Chain next request: DAILY
      ESP32SERIAL.println("CMD:DAILY");
    } 
    else if (response.startsWith("JSON:DAILY:")) {
      Serial.println("\n--- [DAILY FORECAST RECEIVED] ---");
      Serial.println(response.substring(11));

      processIncomingStream(printDailyData, response.substring(11));

      // Chain next request: AQI
      ESP32SERIAL.println("CMD:AQI");
    } 
    else if (response.startsWith("JSON:AQI:")) {
      Serial.println("\n--- [AIR QUALITY RECEIVED] ---");
      Serial.println(response.substring(9));

      processIncomingStream(printAirQualityData, response.substring(9));

      Serial.println("\n*** ALL METRICS SUCCESSFULLY UPDATED ***");
      Serial.println("INPUT NEW CITY.....");

      showMainDashboard();
    } 
    else if (response.startsWith("ERR:")) {
      Serial.print("ERROR FROM ESP32: ");
      Serial.println(response.substring(4));
    }
  }

  int touchX = 0;
  int touchY = 0;
#if defined(XPT_TOUCH)
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    if(orientation == 1) {
      // Map inverted ADC values to display pixels
      touchX = map(p.x, 3800, 300, 0, 480);
      touchY = map(p.y, 3800, 300, 0, 320);

      touchX = constrain(touchX, 0, 480);
      touchY = constrain(touchY, 0, 320);
    }
    else {
      TS_Point p = ts.getPoint();

      // Map inverted ADC values to display pixels
      touchX = map(p.x, 3800, 300, 0, 479);
      touchY = map(p.y, 3800, 300, 0, 319);

      touchX = constrain(touchX, 0, 479);
      touchY = constrain(touchY, 0, 319);
    }
  #else //end XPT Touch
    if (ts.touched())
    {
        // 1. Get raw reading
        TS_Point p = ts.getPoint();

        if(orientation == 1) {
          // 2. Map coordinates for Rotation = 1
          int mappedX = p.y;
          int mappedY = 320 - p.x;

          // 3. Calibrate edges (maps the observed ~0 to ~319 active range to 0-479)
          touchX = map(mappedX, 0, 470, 0, 479);
          touchY = map(mappedY, 0, 310, 0, 319);

          // 4. Constrain to valid display bounds
          touchX = constrain(touchX, 0, 479);
          touchY = constrain(touchY, 0, 319);
        }
        else {
          // 2. Map coordinates for Rotation = 3
          int mappedX = 480 - p.y;
          int mappedY = p.x;

          // 3. Scale and calibrate active edge boundaries
          touchX = map(mappedX, 10, 480, 0, 479);
          touchY = map(mappedY, 0, 310, 0, 319);

          // 4. Constrain bounds
          touchX = constrain(touchX, 0, 479);
          touchY = constrain(touchY, 0, 319);
        }
#endif
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
        delay(50); // Debounce touch
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
  }  // end touch


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
  int btnY = 5;
  int btnW = 50;
  int btnH = 24;

  return (touchX >= btnX && touchX <= (btnX + btnW) &&
          touchY >= btnY && touchY <= (btnY + btnH));
}
#endif