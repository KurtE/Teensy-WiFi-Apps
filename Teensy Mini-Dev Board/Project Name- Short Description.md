# Project Name: Weather App for the Protosupplies Mini Platform For Teensy 4.1 (https://protosupplies.com/product/mini-platform-teensy41/)

The Mini Platform features a ESP32-C3 as well as a ST7796 display with capacitive touch screen.  This allows the ESP32-C3 to handle all the WiFi connections to the Open-Meteo weather API.  The Teensy 4.1 handles the all the display and processing of the data received.  On the Teensy you input the City the you want.  The T4.1 sends the City request to the ESP32C3 which the retrieves and sends the weather data in JSON format back to the T41 for processing and displaying data. 

The selection of the weather city is via the Serial monitor or a display keyboard which is optional configuration.

## 🛠️ Hardware Required
* Protosupplies Mini Platform For Teensy 4.1 with Display and ESP32-C3

## 📌 Pin Wiring Connections

None as the Mini Platform is self contained.

## 📦 Required Libraries
* **ST7796_t3 library** pre-installed when you install Teensy via Board Manager or via Teensyduino
* **Adafruit_FT6236 Touch library**  install via Arduino IDE Library Manager
* **ArduinoJson library** install via Arduino IDE Library Manager
* **ILI9341_t3_Keypad library** you will need to download my modified version which supports the ST7796 and ILI9488 displays.  This can be downloaded at https://github.com/mjs513/ILI9341_t3_Keypad/tree/ST7796_keyboard


## 🚀 How to Use
**ESP32-C3 Sketch:**
1. Clone or download this repository.
2. Open `esp32_teensy_v1.ino` in the Arduino IDE.
3. Edit the `Secrets.h` tab to set up your SSID and PASSWORD
4. Install the required libraries listed above.
5. Select the ESP32C3DevBoard and port, then click **Upload**.

**Teensy 4.1 Sketch**
1. Clone or download this repository.
2. Open `weather_display_v5.ino` in the Arduino IDE.
3. Install the required libraries listed above if not pre-installed.
4. Config app for the display, orientation and use of diaplay keypad (optional). Keypad is optional since you can still input a new city from the serial monitor.
```c++
/**********************************************
*  Setup display and touch
***********************************************/
/**********************************************
*  Setup display and touch
***********************************************/
// Configre ESP32 stack
//#define MINIDEVBRD    //C3 on minidev board
//#define ILI9488_DISP // esle ST7796
#define XPT_TOUCH  //else FT6236
//#define INVERT_ST7796_DISPLAY
#define USE_KEYBOARD
//#define printForecast
#define orientation 3  // or 1 (landscape)

// Configure Display CS, DC and RST pins
// Pin assignments (adjust to your hardware/display setup)
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8

// Config Touch Pins
#if defined(XPT_TOUCH)
#define CS_PIN  7
#define TOUCH_SPI SPI1
#else
#define TOUCH_WIRE Wire
#endif

String DEFAULT_CITY = "Disneyland";
```
6. Select your `Teensy 4.1` and port, then click **Upload**.