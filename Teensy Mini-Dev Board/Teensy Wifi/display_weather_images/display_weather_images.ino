#include "clear.h"
#include "dense-drizzle.h"
#include "dense-freezing-drizzle.h"
#include "fog.h"
#include "heavy-freezing-rain.h"
#include "heavy-rain.h"
#include "heavy-snowfall.h"
#include "light-drizzle.h"
#include "light-freezing-drizzle.h"
#include "light-freezing-rain.h"
#include "light-rain.h"
#include "moderate-drizzle.h"
#include "moderate-rain.h"
#include "moderate-snowfall.h"
#include "mostly-clear.h"
#include "overcast.h"
#include "partly-cloudy.h"
#include "rime-fog.h"
#include "slight-snowfall.h"
#include "snowflake.h"
#include "thunderstorm-with-hail.h"
#include "thunderstorm.h"

#include <ST7796_t3.h> // Hardware-specific library

#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8
#define SD_CS     BUILTIN_SDCARD     // Adjust to your SD CS pin (if using SD card)

//ILI9488_t3 tft = ILI9488_t3(&SPI, TFT_CS, TFT_DC, TFT_RST);
ST7796_t3 tft = ST7796_t3(TFT_CS, TFT_DC, TFT_RST);

uint16_t *images[] = {
	image_clear,
	image_dense_drizzle,
	image_dense_freezing_drizzle,
	image_fog,
	image_heavy_freezing_rain,
	image_heavy_rain,
	image_heavy_snowfall,
	image_light_drizzle,
	image_light_freezing_drizzle,
	image_light_freezing_rain,
	image_light_rain,
	image_moderate_drizzle,
	image_moderat_rain,
	image_moderate_snowfall,
	image_mostly_clear,
	image_overcast,
	image_partly_cloudy,
	image_rime_fog,
	image_slight_snowfall,
	image_snowflake,
	image_thunderstorm_with_hail,
	image_thunderstorm
};

#define COUNT_IMAGES  (sizeof(images) / sizeof(images[0]))


/*************************************************/
// Custom Colors (RGB565 format)
#define COLOR_BG        0x0000 // Black
#define COLOR_WHITE     0xFFFF
#define COLOR_CYAN      0x07FF
#define COLOR_YELLOW    0xFFE0
#define COLOR_ORANGE    0xFD20
#define COLOR_GRAY      0x7BEF


/*************************************************/

void setup() {
  tft.init(320, 480);
  //tft.invertDisplay(true);
  //tft.begin();
  tft.setRotation(1); // Landscape (480x320)
  tft.setOrigin(0,0);
  tft.fillScreen(COLOR_CYAN);
  delay(500);
  tft.fillScreen(COLOR_BG);
  delay(500);

  // put your main code here, to run repeatedly:
  int x = 2; 
  int y = 2;
  for (int i = 0; i < COUNT_IMAGES; i++) {
    tft.writeRect(x, y, 64, 64, images[i]);
    x += 68;
    if (x >= 480) {
      x = 2; 
      y += 66;
    }
  }


}

void loop() {
}
