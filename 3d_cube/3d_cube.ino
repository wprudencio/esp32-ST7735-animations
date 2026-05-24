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

#define LED_PIN   10
#define NUMPIXELS 1

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define WIDTH  160
#define HEIGHT 128
#define CX (WIDTH / 2)
#define CY (HEIGHT / 2)

// ─── 3D Math ────────────────────────────────────────────
typedef struct { float x, y, z; } Vec3;

Vec3 cubeVerts[8] = {
  {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
  {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
};

// 12 edges (pairs of vertex indices)
int edges[12][2] = {
  {0,1},{1,2},{2,3},{3,0},
  {4,5},{5,6},{6,7},{7,4},
  {0,4},{1,5},{2,6},{3,7}
};

// Face colors (6 faces, each made of 2 triangles / 4 edges)
uint16_t faceColors[6] = {
  0xF800, // Red
  0x07E0, // Green
  0x001F, // Blue
  0xFFE0, // Yellow
  0x07FF, // Cyan
  0xF81F  // Magenta
};
int faceVerts[6][4] = {
  {0,1,2,3},  // front
  {5,4,7,6},  // back
  {4,0,3,7},  // left
  {1,5,6,2},  // right
  {3,2,6,7},  // top
  {4,5,1,0}   // bottom
};

float angleX = 0, angleY = 0, angleZ = 0;

// Project 3D point to screen with perspective
void project(Vec3 p, float* sx, float* sy, float* depth) {
  float scale = 1.2;
  float dist = 3.0;
  float z = p.z + dist;
  *depth = z;
  *sx = CX + (p.x / z) * (CX * scale);
  *sy = CY + (p.y / z) * (CX * scale);
}

// Rotate a point around X, Y, Z axes
Vec3 rotate(Vec3 p, float ax, float ay, float az) {
  // Y rotation
  float cosY = cosf(ay), sinY = sinf(ay);
  float x1 = p.x * cosY - p.z * sinY;
  float z1 = p.x * sinY + p.z * cosY;
  p.x = x1; p.z = z1;

  // X rotation
  float cosX = cosf(ax), sinX = sinf(ax);
  float y1 = p.y * cosX - p.z * sinX;
  float z2 = p.y * sinX + p.z * cosX;
  p.y = y1; p.z = z2;

  // Z rotation
  float cosZ = cosf(az), sinZ = sinf(az);
  float x2 = p.x * cosZ - p.y * sinZ;
  float y2 = p.x * sinZ + p.y * cosZ;
  p.x = x2; p.y = y2;

  return p;
}

// ─── Previous state for incremental rendering ───────────
#define MAX_LINES 30
int prevX1[MAX_LINES], prevY1[MAX_LINES];
int prevX2[MAX_LINES], prevY2[MAX_LINES];
bool lineActive[MAX_LINES];
int lineCount = 0;
bool firstFrame = true;

uint16_t lerpColor(uint16_t c1, uint16_t c2, float t) {
  uint8_t r1 = (c1 >> 8) & 0xF8;
  uint8_t g1 = (c1 >> 3) & 0xFC;
  uint8_t b1 = (c1 << 3) & 0xF8;
  uint8_t r2 = (c2 >> 8) & 0xF8;
  uint8_t g2 = (c2 >> 3) & 0xFC;
  uint8_t b2 = (c2 << 3) & 0xF8;
  uint8_t r = r1 + (r2 - r1) * t;
  uint8_t g = g1 + (g2 - g1) * t;
  uint8_t b = b1 + (b2 - b1) * t;
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

  for (int i = 0; i < MAX_LINES; i++) {
    lineActive[i] = false;
    prevX1[i] = prevY1[i] = prevX2[i] = prevY2[i] = 0;
  }
}

void loop() {
  unsigned long t = millis();

  // ─── Rotate ───
  angleX += 0.025;
  angleY += 0.04;
  angleZ += 0.015;

  // ─── Transform vertices ───
  Vec3 transformed[8];
  float sx[8], sy[8], depth[8];
  for (int i = 0; i < 8; i++) {
    transformed[i] = rotate(cubeVerts[i], angleX, angleY, angleZ);
    project(transformed[i], &sx[i], &sy[i], &depth[i]);
  }

  // ─── Color shift over time ───
  uint8_t hueShift = (uint8_t)(t * 0.03);
  uint16_t shiftedColors[6];
  for (int i = 0; i < 6; i++) {
    uint8_t r = (faceColors[i] >> 8) & 0xF8;
    uint8_t g = (faceColors[i] >> 3) & 0xFC;
    uint8_t b = (faceColors[i] << 3) & 0xF8;
    // slight hue shift
    shiftedColors[i] = tft.color565(r, g, b);
  }

  // ─── Sort edges by depth for painter's algorithm ───
  // (simplified: just draw edges, depth-sort face centers for color)
  struct Edge { int v0, v1; float z; uint16_t color; };
  struct Edge sorted[12];

  for (int i = 0; i < 12; i++) {
    int a = edges[i][0], b = edges[i][1];
    sorted[i].v0 = a;
    sorted[i].v1 = b;
    sorted[i].z = (depth[a] + depth[b]) / 2;
    // Assign face color based on which face this edge belongs to
    int faceIdx = i / 2;  // 2 edges per face
    if (faceIdx > 5) faceIdx = 5;
    sorted[i].color = faceColors[faceIdx];
  }

  // Simple bubble sort by depth (farthest first)
  for (int i = 0; i < 12 - 1; i++) {
    for (int j = 0; j < 12 - i - 1; j++) {
      if (sorted[j].z > sorted[j+1].z) {
        struct Edge tmp = sorted[j];
        sorted[j] = sorted[j+1];
        sorted[j+1] = tmp;
      }
    }
  }

  // ─── First frame ───
  if (firstFrame) {
    tft.fillScreen(ST7735_BLACK);
    for (int i = 0; i < 12; i++) {
      int a = sorted[i].v0, b = sorted[i].v1;
      int x1 = sx[a], y1 = sy[a];
      int x2 = sx[b], y2 = sy[b];
      tft.drawLine(x1, y1, x2, y2, sorted[i].color);
      if (i < MAX_LINES) {
        prevX1[i] = x1; prevY1[i] = y1;
        prevX2[i] = x2; prevY2[i] = y2;
        lineActive[i] = true;
      }
    }
    lineCount = 12;
    firstFrame = false;
    goto finish;
  }

  // ─── Incremental: erase old lines, draw new ───
  // We erase ALL old lines first (one pass), then draw ALL new lines
  for (int i = 0; i < lineCount && i < MAX_LINES; i++) {
    if (lineActive[i]) {
      tft.drawLine(prevX1[i], prevY1[i], prevX2[i], prevY2[i], ST7735_BLACK);
    }
  }

  for (int i = 0; i < 12 && i < MAX_LINES; i++) {
    int a = sorted[i].v0, b = sorted[i].v1;
    int x1 = sx[a], y1 = sy[a];
    int x2 = sx[b], y2 = sy[b];
    tft.drawLine(x1, y1, x2, y2, sorted[i].color);
    prevX1[i] = x1; prevY1[i] = y1;
    prevX2[i] = x2; prevY2[i] = y2;
    lineActive[i] = true;
  }
  lineCount = 12;

  // ─── Draw a small glow dot at each vertex ───
  // (only for vertices that aren't too far back)
  for (int i = 0; i < 8; i++) {
    if (depth[i] > 1.0 && depth[i] < 5.0) {
      uint8_t brightness = 255 - (depth[i] - 1.0) / 4.0 * 200;
      tft.drawPixel(sx[i], sy[i], tft.color565(brightness, brightness, brightness));
    }
  }

finish:
  // ─── WS2812: rainbow cycle ───
  uint8_t ledHue = (uint8_t)(t * 0.04);
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

  delay(16);
}
