#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>

// TFT pin definitions
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    3
#define TFT_MOSI  2
#define TFT_SCLK  1

// Input & LED
#define TOUCH_PIN 0
#define LED_PIN   10
#define NUMPIXELS 1

// Colors
#define COLOR_BG     ST7735_BLACK
#define COLOR_HELLO  ST7735_CYAN
#define COLOR_TOUCH  ST7735_GREEN
#define COLOR_RELEASED ST7735_RED

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

bool last_touch = false;

void setup() {
  pinMode(TOUCH_PIN, INPUT);

  pixels.begin();
  pixels.setBrightness(40);
  pixels.clear();
  pixels.show();

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);

  // Draw the static "Hello World" once
  tft.setTextColor(COLOR_HELLO);
  tft.setTextSize(2);
  tft.setCursor(20, 50);
  tft.println("Hello");
  tft.setCursor(20, 75);
  tft.println("World!");

  // Draw static label for touch status
  tft.setTextSize(1);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(20, 110);
  tft.print("Touch: ");
}

void loop() {
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);

  // Update WS2812
  if (touched) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
  } else {
    pixels.clear();
  }
  pixels.show();

  // Update display only on change (save CPU / reduce flicker)
  if (touched != last_touch) {
    tft.fillRect(70, 110, 80, 15, COLOR_BG); // erase previous text

    tft.setTextSize(1);
    if (touched) {
      tft.setTextColor(COLOR_TOUCH);
      tft.setCursor(70, 110);
      tft.print("TOUCHED");
    } else {
      tft.setTextColor(COLOR_RELEASED);
      tft.setCursor(70, 110);
      tft.print("released");
    }
    last_touch = touched;
  }

  delay(30);
}
