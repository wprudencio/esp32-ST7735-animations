#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>

#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    3
#define TFT_MOSI  2
#define TFT_SCLK  1

#define LED_PIN   10
#define NUMPIXELS 1

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define WIDTH  160
#define HEIGHT 128
#define CENTER  (WIDTH / 2)
#define HALF_BARS 12
#define BAR_W 5
#define BAR_GAP 1

float heights[HALF_BARS];
float targets[HALF_BARS];
float phases[HALF_BARS];
int prevTopL[HALF_BARS];
int prevTopR[HALF_BARS];
bool firstFrame = true;

uint16_t hsvToRGB565(uint8_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  uint8_t region = h / 43;
  uint8_t remainder = (h - region * 43) * 6;
  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
  switch (region) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
  return tft.color565(r, g, b);
}

void drawFirstFrame(unsigned long t) {
  int step = BAR_W + BAR_GAP;
  tft.fillScreen(ST7735_BLACK);
  for (int i = 0; i < HALF_BARS; i++) {
    int barH = heights[i] * (HEIGHT - 10);
    if (barH < 1 && heights[i] > 0) barH = 1;
    int barTop = HEIGHT - barH;
    uint8_t hue = (uint8_t)(t * 0.04 + i * 18);
    uint16_t color = hsvToRGB565(hue, 220, 230);
    int xL = CENTER - (i + 1) * step;
    int xR = CENTER + BAR_GAP + i * step;
    if (barH > 0) {
      tft.fillRect(xL, barTop, BAR_W, barH, color);
      tft.fillRect(xR, barTop, BAR_W, barH, color);
    }
    prevTopL[i] = barTop;
    prevTopR[i] = barTop;
  }
  firstFrame = false;
}

void drawIncremental(unsigned long t) {
  int step = BAR_W + BAR_GAP;
  for (int i = 0; i < HALF_BARS; i++) {
    int barH = heights[i] * (HEIGHT - 10);
    if (barH < 1 && heights[i] > 0) barH = 1;
    int barTop = HEIGHT - barH;
    uint8_t hue = (uint8_t)(t * 0.04 + i * 18);
    uint16_t color = hsvToRGB565(hue, 220, 230);
    int xL = CENTER - (i + 1) * step;
    int xR = CENTER + BAR_GAP + i * step;

    if (barTop < prevTopL[i]) {
      tft.fillRect(xL, barTop, BAR_W, prevTopL[i] - barTop, color);
    } else if (barTop > prevTopL[i]) {
      tft.fillRect(xL, prevTopL[i], BAR_W, barTop - prevTopL[i], ST7735_BLACK);
    }
    prevTopL[i] = barTop;

    if (barTop < prevTopR[i]) {
      tft.fillRect(xR, barTop, BAR_W, prevTopR[i] - barTop, color);
    } else if (barTop > prevTopR[i]) {
      tft.fillRect(xR, prevTopR[i], BAR_W, barTop - prevTopR[i], ST7735_BLACK);
    }
    prevTopR[i] = barTop;
  }

  // Center flare
  int flareSize = 2 + (sin(t * 0.006) + 1) * 4;
  uint8_t flareHue = (uint8_t)(t * 0.08);
  tft.fillRect(CENTER - 1, HEIGHT - 12 - flareSize, 2, 6, ST7735_BLACK);
  tft.fillRect(CENTER - 1, HEIGHT - 12 - flareSize, 2, flareSize, hsvToRGB565(flareHue, 255, 255));
}

void setup() {
  pixels.begin();
  pixels.setBrightness(40);
  pixels.clear();
  pixels.show();

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);

  for (int i = 0; i < HALF_BARS; i++) {
    heights[i] = 0;
    targets[i] = 0;
    phases[i] = random(100) * 0.1;
    prevTopL[i] = HEIGHT;
    prevTopR[i] = HEIGHT;
  }
}

void loop() {
  unsigned long t = millis();

  // Compute bar heights
  for (int i = 0; i < HALF_BARS; i++) {
    float freq = 0.5 + i * 0.25;
    float amp = 0.3 + sin(t * 0.002 + phases[i] + sin(t * 0.0007)) * 0.25;
    float wave = sin(t * 0.004 * freq + phases[i]) * 0.35
               + sin(t * 0.009 * freq + phases[i] * 2) * 0.25
               + sin(t * 0.018 * freq + phases[i] * 3) * 0.15
               + sin(t * 0.035 * freq + phases[i] * 4) * 0.1
               + ((float)random(100) / 100.0) * 0.12;
    targets[i] = (amp + wave) * 0.5;
    if (targets[i] < 0.05) targets[i] = 0;
    if (targets[i] > 1.0)  targets[i] = 1.0;
    if (targets[i] > heights[i]) {
      heights[i] += (targets[i] - heights[i]) * 0.55;
    } else {
      heights[i] += (targets[i] - heights[i]) * 0.07;
    }
  }

  // Draw
  if (firstFrame) {
    drawFirstFrame(t);
  } else {
    drawIncremental(t);
  }

  // WS2812 rainbow cycle
  uint8_t ledHue = (uint8_t)(t * 0.05);
  uint8_t lr, lg, lb;
  uint8_t ledRegion = ledHue / 43;
  uint8_t ledRem = (ledHue - ledRegion * 43) * 6;
  switch (ledRegion) {
    case 0: lr = 255; lg = ledRem; lb = 0; break;
    case 1: lr = 255 - ledRem; lg = 255; lb = 0; break;
    case 2: lr = 0; lg = 255; lb = ledRem; break;
    case 3: lr = 0; lg = 255 - ledRem; lb = 255; break;
    case 4: lr = ledRem; lg = 0; lb = 255; break;
    default: lr = 255; lg = 0; lb = 255 - ledRem; break;
  }
  pixels.setPixelColor(0, pixels.Color(lr, lg, lb));
  pixels.show();

  delay(25);
}
