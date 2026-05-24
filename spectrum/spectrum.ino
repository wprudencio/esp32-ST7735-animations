#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>

// TFT pins (Hardware SPI — MOSI=7, SCLK=6 are ESP32-C3 default HSPI)
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
#define NUM_BARS 16

float heights[NUM_BARS];
float targets[NUM_BARS];
float phases[NUM_BARS];

void setup() {
  pixels.begin();
  pixels.setBrightness(40);
  pixels.clear();
  pixels.show();

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);

  for (int i = 0; i < NUM_BARS; i++) {
    heights[i] = 0;
    targets[i] = 0;
    phases[i] = random(100) * 0.1;
  }
}

void loop() {
  // Erase bars area
  tft.fillRect(0, 0, WIDTH, HEIGHT, ST7735_BLACK);

  unsigned long t = millis();

  for (int i = 0; i < NUM_BARS; i++) {
    float freq = 0.5 + i * 0.3;

    // Simulate audio spectrum with multiple sine layers + noise
    float amp = 0.3 + sin(t * 0.002 + phases[i]) * 0.3;
    float wave = sin(t * 0.005 * freq + phases[i]) * 0.4
               + sin(t * 0.011 * freq + phases[i] * 2) * 0.3
               + sin(t * 0.023 * freq + phases[i] * 3) * 0.2
               + ((float)random(100) / 100.0) * 0.15;

    targets[i] = (amp + wave) * 0.5;
    if (targets[i] < 0.05) targets[i] = 0;
    if (targets[i] > 1.0)  targets[i] = 1.0;

    // Attack fast, decay slow (like real spectrum analyzers)
    if (targets[i] > heights[i]) {
      heights[i] += (targets[i] - heights[i]) * 0.6;
    } else {
      heights[i] += (targets[i] - heights[i]) * 0.08;
    }

    int barH = heights[i] * (HEIGHT - 10);
    int barX = i * (WIDTH / NUM_BARS);
    int barW = (WIDTH / NUM_BARS) - 1;

    // Color: warm (red/orange) on left → cool (blue/cyan) on right
    float pos = i / (float)NUM_BARS;
    uint8_t r = (uint8_t)((1.0 - pos) * 255);
    uint8_t g = (uint8_t)(pos < 0.5 ? pos * 2 * 200 : (1.0 - (pos - 0.5) * 2) * 200);
    uint8_t b = (uint8_t)(pos * 255);

    tft.fillRect(barX, HEIGHT - 10 - barH, barW, barH, tft.color565(r, g, b));
  }

  // WS2812 pulses to the beat
  float beat = (sin(t * 0.008) + 1) * 0.5;
  pixels.setPixelColor(0, pixels.Color(
    (uint8_t)(beat * 255),
    (uint8_t)((1.0 - beat) * 128),
    (uint8_t)(beat * 200)
  ));
  pixels.show();

  delay(30);
}
