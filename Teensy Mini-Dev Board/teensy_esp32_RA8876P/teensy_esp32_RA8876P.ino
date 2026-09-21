/**********************************************
*  Setup display and touch
***********************************************/
// Configre ESP32 stack
//#define MINIDEVBOARD    //C3 on minidev board
//#define ILI9488_DISP // esle ST7796
#define RA8876P_DISP
//#define XPT_TOUCH  //else FT6236
//#define INVERT_ST7796_DISPLAY
#define USE_KEYBOARD
//#define printForecast
#define orientation 0

#if !defined(RA8876P_DISP)
// Configure Display CS, DC and RST pins
// Pin assignments (adjust to your hardware/display setup)
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8
#endif

// Config Touch Pins
#if defined(XPT_TOUCH)
#include <XPT2046_RA8876.h>
#define CS_PIN  7
#define TOUCH_SPI SPI1
#else
#define CS_PIN  6
#define TOUCH_WIRE Wire2
#endif

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
#elif defined(RA8876P_DISP)
#include "RA8876_Config_8080.h"
#include "RA8876_common.h"
#include <RA8876_t41_p.h>
#include <math.h>
#else
#include <ST7796_t3.h> // Hardware-specific library
#endif


#if defined(ILI9488_DISP)
ILI9488_t3 tft = ILI9488_t3(&SPI, TFT_CS, TFT_DC, TFT_RST);
#elif defined(RA8876P_DISP)
// RA8876_8080_DC, RA8876_8080_CS and RA8876_8080_RESET are defined in
// src/RA8876_Config_8080.h.
RA8876_t41_p tft = RA8876_t41_p(RA8876_8080_DC,RA8876_8080_CS,RA8876_8080_RESET);

#else
ST7796_t3 tft = ST7796_t3(TFT_CS, TFT_DC, TFT_RST);
#endif

 // Common optimized Teensy driver
#include <PNGdec.h>      // PNG decoder library by bitbank2

#include "font_Arial.h"
#include "font_ArialBold.h"

#if defined(XPT_TOUCH)
#include <XPT2046_Touchscreen.h>
XPT2046_Touchscreen ts(CS_PIN);  // Param 2 - Touch IRQ Pin - interrupt enabled polling
// XPT2046 typical raw ADC limits (adjust if edges are slightly off)
#define TS_MINX 200
#define TS_MAXX 3800
#define TS_MINY 200
#define TS_MAXY 3800
#else
#include <FT5206.h>
#define CTP_INT 6
#define MAXTOUCHLIMIT 1
#endif

#if defined(USE_KEYBOARD)
#if defined(RA8876P_DISP)
#include <RA8876_t4_Keypad.h>
#else
#include <ILI9341_t3_Keypad.h>
#endif

// easy way to include fonts but change globally
#define FONT_BUTTON Arial_12  // font for keypad buttons
#if defined(XPT)
Keyboard MyKeyboard(&tft, &ts);
[[maybe_unused]] uint16_t ScreenLeft = 30, ScreenRight = 468, ScreenTop = 302, ScreenBottom = 3;
#else
Keyboard MyKeyboard(&tft);
#endif
#endif

/*****************************************************
*  Config weather app / ESP32 Connection
******************************************************/
#include "forwardDecs.h"
#include "weatherApp.h"

#if defined(MINIDEVBOARD)
#define ESP32SERIAL Serial7  //7 for C3, 5 for C5/C6
#else
#define ESP32SERIAL Serial1  //7 for C3, 5 for C5/C6
#endif
#define ESP32SERIAL_BUFFER_SIZE 4 * 1024
unsigned char esp32SerialBuffer[ESP32SERIAL_BUFFER_SIZE];

bool esp32Attached = false;
String aweather_city = "Disneyland";

//Auto refresh every hour
uint32_t g_weather_cycle_time_ms = (uint32_t)(60 * 60 * 1000); // cycle time in MS 10 * 60 *1000;
uint32_t g_last_cycle_time_ms = 0;

/***********************************************************/

void setup() {
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
    Serial.println("Ping ESP32-CX...");
    ping_count++;
    if (ping_count % 10) {
      ESP32SERIAL.println("CMD:ESPRESTART");
      Serial.println("ESPRESTART SENT");
      delay(4000);
    }

    ESP32SERIAL.println("?");
    
    uint32_t start = millis();
    while (millis() - start < 500) {
      if (ESP32SERIAL.available()) {
        String res = ESP32SERIAL.readStringUntil('\n');
        res.trim();
        if (res == "Y") {
          esp32Attached = true;
          Serial.println("ESP32-CX Connected!");
          break;
        }
      }
    }
  }

#if defined(ILI9488_DISP)
  tft.begin();
#elif defined(RA8876P_DISP)
  // Set 8/16bit bus mode. Default is 8bit bus mode.
  tft.setBusWidth(RA8876_8080_BUS_WIDTH); // RA8876_8080_BUS_WIDTH is defined in
                                          // src/RA8876_Config_8080.h. 
  tft.begin(BUS_SPEED); // RA8876_8080_BUS_WIDTH is defined in
                        // src/RA8876_Config_8080.h. Default is 20MHz. 
  //tft.graphicMode(true);
#else
  tft.init(320, 480);
  #if defined(INVERT_ST7796_DISPLAY)
    tft.invertDisplay(true);  //black display
  #endif //end invert display
#endif // Intialize display
  tft.setRotation(orientation); // Landscape (480x320)
  tft.setOrigin(0,0);
  tft.fillScreen(COLOR_BG);
  tft.setFont(Arial_10);

  Serial.println("initialization done.");

#if defined(XPT_TOUCH)
  ts.begin(1024,600);
  // Replace these for your screen module
  ts.setCalibration(1921, 1974, 171, 70);
#elif defined(RA8876P_DISP)
  tft.setWireObject(&Wire2);
  tft.useCapINT(CTP_INT);//we use the capacitive chip Interrupt out!
  //the following set the max touches (max 5)
  tft.setTouchLimit(MAXTOUCHLIMIT);
  tft.enableCapISR(true);//capacitive touch screen interrupt it's armed
#endif

#if defined(USE_KEYBOARD)
  MyKeyboard.init(COLOR_BLACK, COLOR_WHITE, COLOR_BLUE, COLOR_DARKGREY, COLOR_DARKGREY, COLOR_NAVY, COLOR_BLACK, FONT_BUTTON);
#if defined(XPT)
  MyKeyboard.setTouchLimits( ScreenLeft, ScreenRight, ScreenTop, ScreenBottom);
#endif
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
#if defined(USE_XPT)
  if (ts.isTouching()) {

    ts.getPosition(touchX, touchY);
    touchX = constrain(touchX, 0, 1024);
    touchY = constrain(touchY, 0, 600);
#else
 if (tft.touched()) { //if touched(true) detach isr
    //at this point we need to fill the FT5206 registers...
    tft.updateTS();//now we have the data inside library
    //Serial.print(">> touches:");
    //Serial.print(tft.getTouches());
    //Serial.print(" | gesture:");
    //Serial.print(tft.getGesture(), HEX);
    //Serial.print(" | state:");
    //Serial.print(tft.getTouchState(), HEX);
    uint16_t coordinates[MAXTOUCHLIMIT][2];//to hold coordinates
    tft.getTScoordinates(coordinates);//done
    //now coordinates has the x,y of all touches
    //for (uint8_t i = 0; i <= tft.getTouches(); i++) {
    //  Serial.printf(" (%d,%d)", coordinates[i][0], coordinates[i][1]);
    //}
    touchX =  tft.width() - coordinates[0][0];
    touchY =  tft.height() - coordinates[0][1];
    tft.enableCapISR();//rearm ISR if needed (touched(true))
    //Serial.println();
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
    delay(50);
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