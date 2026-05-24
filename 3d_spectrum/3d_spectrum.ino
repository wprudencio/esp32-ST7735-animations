#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

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
#define CX (WIDTH / 2)
#define CY (HEIGHT / 2)
#define NUM_BARS 16

// ─── 3D bar in world space ──────────
float barAngles[NUM_BARS];    // angle around Y axis
float barHeights[NUM_BARS];   // current height (simulated audio)
float barTargets[NUM_BARS];   // target height
float barPhases[NUM_BARS];    // for audio simulation

// ─── Camera ─────────────────────────
float camAngle = 0;
float camDist = 4.0;

// ─── Palette ────────────────────────
#define NUM_PALETTES 5
int palette = 0;
bool lastTouch = false;

// ─── Previous screen positions ──────
int prevX1[NUM_BARS], prevY1[NUM_BARS];
int prevX2[NUM_BARS], prevY2[NUM_BARS];
int prevX3[NUM_BARS], prevY3[NUM_BARS];
int prevX4[NUM_BARS], prevY4[NUM_BARS];
bool prevActive[NUM_BARS];
bool firstFrame = true;

// ─── 3D to screen ───────────────────
void project3D(float wx, float wy, float wz, int* sx, int* sy) {
  // Rotate around Y axis (camera orbit)
  float cosA = cosf(camAngle);
  float sinA = sinf(camAngle);
  float rx = wx * cosA - wz * sinA;
  float rz = wx * sinA + wz * cosA;

  // Perspective
  float dist = 2.5;
  float viewZ = rz + camDist;
  if (viewZ < 0.3) viewZ = 0.3;
  float scale = dist / viewZ;

  *sx = CX + rx * scale * 50;
  *sy = CY - wy * scale * 50;
}

// ─── Get palette color ──────────────
uint16_t paletteColor(int idx, float heightNorm, bool dark) {
  float t = (float)idx / NUM_BARS;
  uint8_t r = 0, g = 0, b = 0;
  float bright = dark ? 0.35 : (0.6 + heightNorm * 0.4);

  switch (palette) {
    case 0: { // Rainbow
      uint8_t hue = (uint8_t)(t * 255 + millis() * 0.02);
      if (hue < 85)      { r = 255 - hue * 3; g = hue * 3; }
      else if (hue < 170){ hue -= 85; r = 0; g = 255 - hue * 3; b = hue * 3; }
      else               { hue -= 170; r = hue * 3; b = 255 - hue * 3; }
      break;
    }
    case 1: { // Fire
      r = 255; g = (uint8_t)((1.0 - t) * 200 + heightNorm * 55); break;
    }
    case 2: { // Ice
      r = (uint8_t)(t * 80); g = (uint8_t)((1.0 - t) * 200 + 55); b = 255; break;
    }
    case 3: { // Neon
      r = (uint8_t)(sin(t * 6.28 + millis() * 0.003) * 127 + 128);
      g = (uint8_t)(sin(t * 6.28 + 2.09 + millis() * 0.003) * 127 + 128);
      b = (uint8_t)(sin(t * 6.28 + 4.19 + millis() * 0.003) * 127 + 128);
      break;
    }
    case 4: { // Gold
      r = 255; g = (uint8_t)((1.0 - t) * 180 + 40); b = (uint8_t)(t * 40); break;
    }
  }
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
    barAngles[i] = -1.8 + i * (3.6 / NUM_BARS);
    barHeights[i] = 0;
    barTargets[i] = 0;
    barPhases[i] = random(100) * 0.1;
    prevActive[i] = false;
  }
}

void loop() {
  unsigned long t = millis();

  // ─── Touch: cycle palette ───
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);
  if (touched && !lastTouch) {
    palette = (palette + 1) % NUM_PALETTES;
    firstFrame = true;
    delay(200);
  }
  lastTouch = touched;

  // ─── Camera rotates slowly ───
  camAngle += 0.008;

  // ─── Simulate audio ───
  for (int i = 0; i < NUM_BARS; i++) {
    float freq = 0.5 + i * 0.3;
    float amp = 0.3 + sin(t * 0.0018 + barPhases[i]) * 0.3;
    float wave = sin(t * 0.004 * freq + barPhases[i]) * 0.4
               + sin(t * 0.009 * freq + barPhases[i] * 2) * 0.3
               + sin(t * 0.019 * freq + barPhases[i] * 3) * 0.2
               + ((float)random(100) / 100.0) * 0.1;

    barTargets[i] = (amp + wave) * 0.5;
    if (barTargets[i] < 0.05) barTargets[i] = 0;
    if (barTargets[i] > 1.0)  barTargets[i] = 1.0;

    if (barTargets[i] > barHeights[i]) {
      barHeights[i] += (barTargets[i] - barHeights[i]) * 0.55;
    } else {
      barHeights[i] += (barTargets[i] - barHeights[i]) * 0.07;
    }
  }

  // ─── Draw ─────────────────────

  // First compute all screen positions
  int sx1[NUM_BARS], sy1[NUM_BARS]; // top-left of bar
  int sx2[NUM_BARS], sy2[NUM_BARS]; // top-right
  int sx3[NUM_BARS], sy3[NUM_BARS]; // bottom-right
  int sx4[NUM_BARS], sy4[NUM_BARS]; // bottom-left
  float barDepth[NUM_BARS];         // Z depth for sorting

  for (int i = 0; i < NUM_BARS; i++) {
    float h = barHeights[i] * 1.2;
    float halfW = 0.08;

    // 4 corners of the bar in world space (standing on "ground")
    float wx1 = cosf(barAngles[i]) - halfW;
    float wz1 = sinf(barAngles[i]);
    float wx2 = cosf(barAngles[i]) + halfW;
    float wz2 = sinf(barAngles[i]);

    float viewZ1, viewZ2;
    // We need depth for sorting - compute projected Z
    float cosA = cosf(camAngle);
    float sinA = sinf(camAngle);
    float rz1 = wx1 * sinA + wz1 * cosA;

    barDepth[i] = rz1;

    project3D(wx1, 0, wz1, &sx4[i], &sy4[i]);  // bottom-left
    project3D(wx2, 0, wz2, &sx3[i], &sy3[i]);  // bottom-right
    project3D(wx1, h, wz1, &sx1[i], &sy1[i]);  // top-left
    project3D(wx2, h, wz2, &sx2[i], &sy2[i]);  // top-right
  }

  // Sort bars by depth (painter's algorithm) — far to near
  int order[NUM_BARS];
  float depths[NUM_BARS];
  for (int i = 0; i < NUM_BARS; i++) {
    order[i] = i;
    depths[i] = barDepth[i];
  }
  // Simple insertion sort
  for (int i = 1; i < NUM_BARS; i++) {
    int key = order[i];
    float keyD = depths[i];
    int j = i - 1;
    while (j >= 0 && depths[j] > keyD) {
      order[j+1] = order[j];
      depths[j+1] = depths[j];
      j--;
    }
    order[j+1] = key;
    depths[j+1] = keyD;
  }

  if (firstFrame) {
    // First frame: full draw
    tft.fillScreen(ST7735_BLACK);
    for (int idx = 0; idx < NUM_BARS; idx++) {
      int i = order[idx];
      if (barHeights[i] < 0.01) continue;
      uint16_t c = paletteColor(i, barHeights[i], false);
      uint16_t cDark = paletteColor(i, barHeights[i], true);
      tft.fillTriangle(sx1[i], sy1[i], sx2[i], sy2[i], sx4[i], sy4[i], c);
      tft.fillTriangle(sx2[i], sy2[i], sx3[i], sy3[i], sx4[i], sy4[i], c);
      tft.fillTriangle(sx1[i], sy1[i], sx2[i], sy2[i], 
                       sx1[i] - (sx2[i]-sx1[i])/3, sy1[i] - 3, cDark);
    }
    firstFrame = false;
  } else {
    // ─── Erase all old bars ─────
    for (int i = 0; i < NUM_BARS; i++) {
      if (!prevActive[i]) continue;
      int minX = prevX1[i], maxX = prevX1[i];
      int minY = prevY1[i], maxY = prevY1[i];
      int pts[8] = {prevX1[i], prevY1[i], prevX2[i], prevY2[i],
                    prevX3[i], prevY3[i], prevX4[i], prevY4[i]};
      for (int p = 0; p < 8; p += 2) {
        if (pts[p] < minX) minX = pts[p];
        if (pts[p] > maxX) maxX = pts[p];
        if (pts[p+1] < minY) minY = pts[p+1];
        if (pts[p+1] > maxY) maxY = pts[p+1];
      }
      minY -= 4; minX -= 2; maxX += 2;
      if (minX < 0) minX = 0;
      if (minY < 0) minY = 0;
      if (maxX >= WIDTH) maxX = WIDTH - 1;
      if (maxY >= HEIGHT) maxY = HEIGHT - 1;
      tft.fillRect(minX, minY, maxX - minX + 1, maxY - minY + 1, ST7735_BLACK);
    }

    // ─── Draw all new bars (back to front) ───
    for (int idx = 0; idx < NUM_BARS; idx++) {
      int i = order[idx];
      if (barHeights[i] < 0.01) {
        prevActive[i] = false;
        continue;
      }
      uint16_t c = paletteColor(i, barHeights[i], false);
      uint16_t cDark = paletteColor(i, barHeights[i], true);
      tft.fillTriangle(sx1[i], sy1[i], sx2[i], sy2[i], sx4[i], sy4[i], c);
      tft.fillTriangle(sx2[i], sy2[i], sx3[i], sy3[i], sx4[i], sy4[i], c);
      int topX = sx1[i] - (sx2[i] - sx1[i]) / 3;
      int topY = sy1[i] - 3;
      tft.fillTriangle(sx1[i], sy1[i], sx2[i], sy2[i], topX, topY, cDark);
      tft.drawLine(sx1[i], sy1[i], sx2[i], sy2[i], tft.color565(255, 255, 255));
      tft.drawLine(sx1[i], sy1[i], sx4[i], sy4[i], tft.color565(200, 200, 200));
      prevX1[i] = sx1[i]; prevY1[i] = sy1[i];
      prevX2[i] = sx2[i]; prevY2[i] = sy2[i];
      prevX3[i] = sx3[i]; prevY3[i] = sy3[i];
      prevX4[i] = sx4[i]; prevY4[i] = sy4[i];
      prevActive[i] = true;
    }
  }

  // ─── HUD ──────────────────────
  const char* names[NUM_PALETTES] = {"RAINBOW", "FIRE", "ICE", "NEON", "GOLD"};
  tft.fillRect(0, HEIGHT - 10, WIDTH, 10, ST7735_BLACK);
  tft.setTextColor(tft.color565(180, 180, 180), ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(2, HEIGHT - 9);
  tft.print("[");
  tft.print(palette + 1);
  tft.print("/");
  tft.print(NUM_PALETTES);
  tft.print("] ");
  tft.print(names[palette]);

  // Ground line
  tft.drawLine(0, CY + 10, WIDTH, CY + 10, tft.color565(30, 30, 60));

  // ─── WS2812 ───
  uint16_t lc = paletteColor((t / 80) % NUM_BARS, 0.5 + sin(t * 0.003) * 0.3, false);
  pixels.setPixelColor(0, pixels.Color(
    (lc >> 8) & 0xF8, (lc >> 3) & 0xFC, (lc << 3) & 0xF8));
  pixels.show();

  delay(25);
}
