#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>

#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    3
#define TFT_MOSI  2
#define TFT_SCLK  1

#define TOUCH_PIN 0
#define LED_PIN   10
#define NUMPIXELS 1

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define WIDTH  160
#define HEIGHT 128
#define NUM_BARS 12
#define BAR_W (WIDTH / NUM_BARS - 1)
#define BAR_GAP 1
#define BAR_BOTTOM (HEIGHT - 5)

// ─── 3D box depth (in pixels) ──────
#define DEPTH_X 3   // side face width
#define DEPTH_Y 3   // top face height

// ─── Color palettes ────────────────
#define NUM_PALETTES 5
int currentPalette = 0;
bool lastTouch = false;

// Pre-computed 3D face tints
const float frontBright = 1.0;
const float topBright   = 0.6;
const float sideBright  = 0.35;

// ─── Bar state ─────────────────────
float heights[NUM_BARS];
float targets[NUM_BARS];
float phases[NUM_BARS];
int prevFrontH[NUM_BARS];     // previous front face height
int prevFrontY[NUM_BARS];     // previous front face Y
int prevTopY[NUM_BARS];       // previous top edge Y
bool firstFrame = true;

// ─── Get palette color ─────────────
// palette 0: rainbow, 1: fire, 2: ocean, 3: neon, 4: sunset
uint16_t getColor(int palette, int barIdx, float heightNorm, int face) {
  // face: 0=front, 1=top, 2=side
  float t = (float)barIdx / NUM_BARS;
  uint8_t r = 0, g = 0, b = 0;

  switch (palette) {
    case 0: { // Rainbow
      uint8_t hue = (uint8_t)(t * 255);
      if (hue < 85)      { r = 255 - hue * 3; g = hue * 3; b = 0; }
      else if (hue < 170){ hue -= 85; r = 0; g = 255 - hue * 3; b = hue * 3; }
      else               { hue -= 170; r = hue * 3; g = 0; b = 255 - hue * 3; }
      break;
    }
    case 1: { // Fire
      r = 255;
      g = (uint8_t)((1.0 - t) * 200);
      b = 0;
      break;
    }
    case 2: { // Ocean
      r = 0;
      g = (uint8_t)((1.0 - t) * 180 + 40);
      b = (uint8_t)(t * 200 + 55);
      break;
    }
    case 3: { // Neon
      r = (uint8_t)(sin(t * 6.28) * 127 + 128);
      g = (uint8_t)(sin(t * 6.28 + 2.09) * 127 + 128);
      b = (uint8_t)(sin(t * 6.28 + 4.19) * 127 + 128);
      break;
    }
    case 4: { // Sunset
      r = 255;
      g = (uint8_t)((1.0 - t) * 120);
      b = (uint8_t)(t * 150);
      break;
    }
  }

  // Apply face brightness
  float bright = (face == 0) ? frontBright : (face == 1) ? topBright : sideBright;
  r = (uint8_t)(r * bright);
  g = (uint8_t)(g * bright);
  b = (uint8_t)(b * bright);
  return tft.color565(r, g, b);
}

void setup() {
  pixels.begin();
  pixels.setBrightness(30);
  pixels.clear();
  pixels.show();

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);

  pinMode(TOUCH_PIN, INPUT);

  for (int i = 0; i < NUM_BARS; i++) {
    heights[i] = 0;
    targets[i] = 0;
    phases[i] = random(100) * 0.1;
    prevFrontH[i] = 0;
    prevFrontY[i] = BAR_BOTTOM;
    prevTopY[i] = BAR_BOTTOM;
  }
}

void loop() {
  unsigned long t = millis();

  // ─── Touch: cycle palette ───
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);
  if (touched && !lastTouch) {
    currentPalette = (currentPalette + 1) % NUM_PALETTES;
    firstFrame = true;  // force full redraw
    delay(200);
  }
  lastTouch = touched;

  // ─── Compute bar heights (simulated audio) ───
  for (int i = 0; i < NUM_BARS; i++) {
    float freq = 0.5 + i * 0.35;
    float amp = 0.3 + sin(t * 0.0018 + phases[i]) * 0.3;
    float wave = sin(t * 0.004 * freq + phases[i]) * 0.4
               + sin(t * 0.009 * freq + phases[i] * 2) * 0.3
               + sin(t * 0.019 * freq + phases[i] * 3) * 0.2
               + sin(t * 0.038 * freq + phases[i] * 4) * 0.1
               + ((float)random(100) / 100.0) * 0.1;

    targets[i] = (amp + wave) * 0.5;
    if (targets[i] < 0.05) targets[i] = 0;
    if (targets[i] > 1.0)  targets[i] = 1.0;

    if (targets[i] > heights[i]) {
      heights[i] += (targets[i] - heights[i]) * 0.55;
    } else {
      heights[i] += (targets[i] - heights[i]) * 0.07;
    }
  }

  // ─── Draw ───
  if (firstFrame) {
    tft.fillScreen(ST7735_BLACK);
  }

  for (int i = 0; i < NUM_BARS; i++) {
    int bx = i * (WIDTH / NUM_BARS);
    int barH = heights[i] * (BAR_BOTTOM - 10);
    if (barH < 1 && heights[i] > 0) barH = 1;
    int frontTop = BAR_BOTTOM - barH;   // top of front face
    int topY = frontTop - DEPTH_Y;      // top of top face (further up)
    int sideX = bx + BAR_W;             // right edge of front face

    // ─── First frame: draw full 3D box ───
    if (firstFrame) {
      uint16_t cFront = getColor(currentPalette, i, heights[i], 0);
      uint16_t cTop   = getColor(currentPalette, i, heights[i], 1);
      uint16_t cSide  = getColor(currentPalette, i, heights[i], 2);

      // Front face
      if (barH > 0)
        tft.fillRect(bx, frontTop, BAR_W, barH, cFront);
      // Top face (parallelogram)
      if (barH > 0)
        tft.fillRect(bx, topY, BAR_W, DEPTH_Y, cTop);
      // Side face
      if (barH > 0)
        tft.fillRect(sideX, frontTop, DEPTH_X, barH, cSide);
      // Outline
      tft.drawRect(bx, frontTop, BAR_W, barH, tft.color565(255,255,255));
      tft.drawLine(bx, topY, bx + DEPTH_X, frontTop - DEPTH_Y, tft.color565(200,200,200));

      prevFrontH[i] = barH;
      prevFrontY[i] = frontTop;
      prevTopY[i] = topY;
      continue;
    }

    // ─── Incremental: only changed areas ───

    // 1. Erase old bar (3D box area + margin)
    int oldTop = prevTopY[i];
    int oldFrontTop = prevFrontY[i];
    int oldH = prevFrontH[i];
    int eraseX = bx - 1;
    int eraseW = BAR_W + DEPTH_X + 2;
    int eraseTop = (topY < oldTop) ? topY : oldTop;
    if (eraseTop < 0) eraseTop = 0;
    int eraseBot = BAR_BOTTOM;
    int eraseH = eraseBot - eraseTop;
    if (eraseH > 0) tft.fillRect(eraseX, eraseTop, eraseW, eraseH, ST7735_BLACK);

    // 2. Skip if bar is invisible
    if (barH == 0) {
      prevFrontH[i] = 0;
      prevFrontY[i] = BAR_BOTTOM;
      prevTopY[i] = BAR_BOTTOM;
      continue;
    }

    // 3. Draw new 3D box
    uint16_t cFront = getColor(currentPalette, i, heights[i], 0);
    uint16_t cTop   = getColor(currentPalette, i, heights[i], 1);
    uint16_t cSide  = getColor(currentPalette, i, heights[i], 2);

    // Front face
    tft.fillRect(bx, frontTop, BAR_W, barH, cFront);
    // Top face (3D extrusion upward + right)
    tft.fillRect(bx, topY, BAR_W, DEPTH_Y, cTop);
    // Side face (3D extrusion to the right)
    tft.fillRect(sideX, frontTop, DEPTH_X, barH, cSide);
    // White outline on front face
    tft.drawRect(bx, frontTop, BAR_W, barH, tft.color565(255, 255, 255));
    // Top-left edge (connecting front top to top face back)
    tft.drawLine(bx, topY, bx + DEPTH_X, frontTop - DEPTH_Y, tft.color565(200, 200, 200));

    prevFrontH[i] = barH;
    prevFrontY[i] = frontTop;
    prevTopY[i] = topY;
  }

  firstFrame = false;

  // ─── Palette name overlay (bottom) ───
  const char* names[NUM_PALETTES] = {"RAINBOW", "FIRE", "OCEAN", "NEON", "SUNSET"};
  tft.fillRect(0, HEIGHT - 10, WIDTH, 10, ST7735_BLACK);
  tft.setTextColor(tft.color565(200, 200, 200), ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(2, HEIGHT - 9);
  tft.print("[");
  tft.print(currentPalette + 1);
  tft.print("/");
  tft.print(NUM_PALETTES);
  tft.print("] ");
  tft.print(names[currentPalette]);

  // ─── WS2812: palette-matching color ───
  uint16_t ledCol = getColor(currentPalette, (t / 50) % NUM_BARS, 
                             0.5 + sin(t * 0.003) * 0.3, 0);
  pixels.setPixelColor(0, pixels.Color(
    (ledCol >> 8) & 0xF8,
    (ledCol >> 3) & 0xFC,
    (ledCol << 3) & 0xF8
  ));
  pixels.show();

  delay(25);
}
