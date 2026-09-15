# Project Name: Weather Application for Teensy 4/Sparkfun WiFi Board

This application is meant to test the Sparkfun WiFI/BLE Wireless Add-on.  It also provides a useful application you can use to get the weather for any location on earth.

## 🛠️ Hardware Required
* Teensy 4.1
* Sparkfun Wireless Adapter
* ILI9488 or ST7796 display with a resistive touch screen.

## 📌 Pin Wiring Connections for Display

| Component | Arduino Pin   | Component Pin |
| :-------- | :------------ | :------------ |
| Display   | 3.3V          | VCC           |
|           | GND           | GND           |
|           | RST           | 8             |
|           | DC            | 9             |
|           | CS            | 10            |
|           | MOSI          | 11            |
|           | LED           | 5V            |
|           | MISO          | 12            |
|           | CLK           | 13            |
|           | T_CS          | 7             |
|           | T_MISO (SPI1) | 1             |
|           | T_MOSI        | 26            |
|           | T_CLK         | 27            |

## 📦 Required Libraries
* **ILI9488_t3 library** pre-installed when you install Teensy via Board Manager or via Teensyduino
* **ST7796_t3 library** pre-installed when you install Teensy via Board Manager or via Teensyduino
* **XPT2046_Touch library**  pre-installed when you install Teensy via Board Manager or via Teensyduino
* **ArduinoJson library** install via Arduino IDE Library Manager
* **ILI9341_t3_Keypad library** you will need to download my modified version which supports the ST7796 and ILI9488 displays.  This can be downloaded at https://github.com/mjs513/ILI9341_t3_Keypad/tree/ST7796_keyboard

## 🚀 How to Use
1. Clone or download this repository.
2. Open `weather_display_v5.ino` in the Arduino IDE.
3. Install the required libraries listed above if not pre-installed.
4. Config app for the display, orientation and use of diaplay keypad (optional). Keypad is optional since you can still input a new city from the serial monitor.
```c++
#define ILI488_DISP  //comment out if you want to use ST7796
#define USE_KEYBOARD //comment out if you do not want to use display keyboard
//#define printForecast
#define orientation 3  // or 1 (landscape)
```
6. Select your board and port, then click **Upload**.

Enjoy.
