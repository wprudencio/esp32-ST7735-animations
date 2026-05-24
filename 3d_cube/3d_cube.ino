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
#define NUM_PARTICLES 40

// ─── 3D types ──────────────────────
typedef struct { float x, y, z; } Vec3;

Vec3 cubeVerts[8] = {
  {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
  {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
};
int edges[12][2] = {
  {0,1},{1,2},{2,3},{3,0},
  {4,5},{5,6},{6,7},{7,4},
  {0,4},{1,5},{2,6},{3,7}
};

// ─── Particles ─────────────────────
Vec3 partPos[NUM_PARTICLES];
Vec3 partVel[NUM_PARTICLES];
int prevPx[NUM_PARTICLES], prevPy[NUM_PARTICLES];

// ─── Rotation state ────────────────
float angleX = 0, angleY = 0, angleZ = 0;
int dir = 1;
bool lastTouch = false;
bool firstFrame = true;

// ─── Cube line tracking ────────────
#define MAX_LINES 30
int lx1[MAX_LINES], ly1[MAX_LINES], lx2[MAX_LINES], ly2[MAX_LINES];
bool lineActive[MAX_LINES];
int lineCount = 0;

// ─── 3D math helpers ───────────────
void rotateVec(Vec3* p, float ax, float ay, float az) {
  float cosY = cosf(ay), sinY = sinf(ay);
  float x1 = p->x * cosY - p->z * sinY;
  float z1 = p->x * sinY + p->z * cosY;
  p->x = x1; p->z = z1;

  float cosX = cosf(ax), sinX = sinf(ax);
  float y1 = p->y * cosX - p->z * sinX;
  float z2 = p->y * sinX + p->z * cosX;
  p->y = y1; p->z = z2;

  float cosZ = cosf(az), sinZ = sinf(az);
  float x2 = p->x * cosZ - p->y * sinZ;
  float y2 = p->x * sinZ + p->y * cosZ;
  p->x = x2; p->y = y2;
}

void inverseRotateVec(Vec3* p, float ax, float ay, float az) {
  float cosZ = cosf(-az), sinZ = sinf(-az);
  float x2 = p->x * cosZ - p->y * sinZ;
  float y2 = p->x * sinZ + p->y * cosZ;
  p->x = x2; p->y = y2;
  float cosX = cosf(-ax), sinX = sinf(-ax);
  float y1 = p->y * cosX - p->z * sinX;
  float z2 = p->y * sinX + p->z * cosX;
  p->y = y1; p->z = z2;
  float cosY = cosf(-ay), sinY = sinf(-ay);
  float x1 = p->x * cosY - p->z * sinY;
  float z1 = p->x * sinY + p->z * cosY;
  p->x = x1; p->z = z1;
}

void project(Vec3 p, int* sx, int* sy, float* depth) {
  float scale = 1.2, dist = 3.0;
  float z = p.z + dist;
  *depth = z;
  *sx = CX + (p.x / z) * (CX * scale);
  *sy = CY + (p.y / z) * (CX * scale);
}

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pixels.begin(); pixels.setBrightness(30); pixels.clear(); pixels.show();
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB); tft.setRotation(1); tft.fillScreen(ST7735_BLACK);

  // Init particles inside the cube with random positions & velocities
  for (int i = 0; i < NUM_PARTICLES; i++) {
    partPos[i].x = ((float)random(1000) / 500.0 - 1.0) * 0.85;
    partPos[i].y = ((float)random(1000) / 500.0 - 1.0) * 0.85;
    partPos[i].z = ((float)random(1000) / 500.0 - 1.0) * 0.85;
    partVel[i].x = ((float)random(1000) / 500.0 - 1.0) * 0.005;
    partVel[i].y = ((float)random(1000) / 500.0 - 1.0) * 0.005;
    partVel[i].z = ((float)random(1000) / 500.0 - 1.0) * 0.005;
    prevPx[i] = prevPy[i] = -1;
  }
  for (int i = 0; i < MAX_LINES; i++) lineActive[i] = false;
}

void loop() {
  unsigned long t = millis();

  // ─── Touch: reverse rotation ───
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);
  if (touched && !lastTouch) { dir = -dir; delay(200); }
  lastTouch = touched;

  float da = 0.025 * dir;
  angleX += da; angleY += da * 1.6; angleZ += da * 0.6;

  // ─── Update particles (world-space physics) ───
  for (int i = 0; i < NUM_PARTICLES; i++) {
    // Gravity (world-space -Y)
    partVel[i].y -= 0.0015;

    // Damping
    partVel[i].x *= 0.995;
    partVel[i].y *= 0.995;
    partVel[i].z *= 0.995;

    // Random jitter (liquid thermal motion)
    partVel[i].x += ((float)random(1000) / 5000.0 - 0.1) * 0.0008;
    partVel[i].y += ((float)random(1000) / 5000.0 - 0.1) * 0.0008;
    partVel[i].z += ((float)random(1000) / 5000.0 - 0.1) * 0.0008;

    // Move
    partPos[i].x += partVel[i].x;
    partPos[i].y += partVel[i].y;
    partPos[i].z += partVel[i].z;

    // Check cube bounds: transform to cube-local space, clamp, reflect, go back to world
    Vec3 local = partPos[i];
    inverseRotateVec(&local, angleX, angleY, angleZ);

    bool hit = false;
    if (local.x > 0.9) { local.x = 0.9 - (local.x - 0.9); partVel[i].x = -partVel[i].x * 0.7; hit = true; }
    if (local.x < -0.9) { local.x = -0.9 + (-0.9 - local.x); partVel[i].x = -partVel[i].x * 0.7; hit = true; }
    if (local.y > 0.9) { local.y = 0.9 - (local.y - 0.9); partVel[i].y = -partVel[i].y * 0.7; hit = true; }
    if (local.y < -0.9) { local.y = -0.9 + (-0.9 - local.y); partVel[i].y = -partVel[i].y * 0.7; hit = true; }
    if (local.z > 0.9) { local.z = 0.9 - (local.z - 0.9); partVel[i].z = -partVel[i].z * 0.7; hit = true; }
    if (local.z < -0.9) { local.z = -0.9 + (-0.9 - local.z); partVel[i].z = -partVel[i].z * 0.7; hit = true; }

    if (hit) {
      // Convert back to world space
      rotateVec(&local, angleX, angleY, angleZ);
      partPos[i] = local;
    }
  }

  // ─── Transform cube vertices ───
  Vec3 tv[8]; int sx[8], sy[8]; float sd[8];
  for (int i = 0; i < 8; i++) {
    tv[i] = cubeVerts[i]; rotateVec(&tv[i], angleX, angleY, angleZ);
    project(tv[i], &sx[i], &sy[i], &sd[i]);
  }

  // ─── Sort edges by depth ───
  struct Edge { int v0, v1; float z; };
  struct Edge sorted[12];
  for (int i = 0; i < 12; i++) {
    int a = edges[i][0], b = edges[i][1];
    sorted[i].v0 = a; sorted[i].v1 = b;
    sorted[i].z = (sd[a] + sd[b]) / 2;
  }
  for (int i = 0; i < 11; i++) {
    for (int j = 0; j < 11 - i; j++) {
      if (sorted[j].z > sorted[j+1].z) {
        struct Edge tmp = sorted[j]; sorted[j] = sorted[j+1]; sorted[j+1] = tmp;
      }
    }
  }

  // ─── Project particles ───
  int px[NUM_PARTICLES], py[NUM_PARTICLES];
  float pd[NUM_PARTICLES];
  for (int i = 0; i < NUM_PARTICLES; i++) {
    project(partPos[i], &px[i], &py[i], &pd[i]);
  }

  // ─── DRAW ──────────────────────────
  if (firstFrame) {
    tft.fillScreen(ST7735_BLACK);
    for (int i = 0; i < 12; i++) {
      int a = sorted[i].v0, b = sorted[i].v1;
      tft.drawLine(sx[a], sy[a], sx[b], sy[b], tft.color565(100, 200, 255));
      if (i < MAX_LINES) {
        lx1[i] = sx[a]; ly1[i] = sy[a]; lx2[i] = sx[b]; ly2[i] = sy[b];
        lineActive[i] = true;
      }
    }
    lineCount = 12;
    for (int i = 0; i < NUM_PARTICLES; i++) {
      if (pd[i] > 0.5 && pd[i] < 6.0) {
        uint8_t b = (1.0 - (pd[i] - 0.5) / 5.5) * 255;
        tft.fillCircle(px[i], py[i], 1 + (uint8_t)((1.0 / pd[i]) * 4), tft.color565(0, b / 2, b));
        prevPx[i] = px[i]; prevPy[i] = py[i];
      }
    }
    firstFrame = false;
  } else {
    // Erase old particles
    for (int i = 0; i < NUM_PARTICLES; i++) {
      if (prevPx[i] >= 0) {
        tft.fillCircle(prevPx[i], prevPy[i], 3, ST7735_BLACK);
      }
    }
    // Erase old cube lines
    for (int i = 0; i < lineCount && i < MAX_LINES; i++) {
      if (lineActive[i]) tft.drawLine(lx1[i], ly1[i], lx2[i], ly2[i], ST7735_BLACK);
    }
    // Draw cube lines (with cyan glow)
    for (int i = 0; i < 12 && i < MAX_LINES; i++) {
      int a = sorted[i].v0, b = sorted[i].v1;
      float edgeDepth = (sd[a] + sd[b]) / 2;
      uint8_t cb = (uint8_t)((1.0 - (edgeDepth - 0.5) / 5.5) * 200 + 55);
      tft.drawLine(sx[a], sy[a], sx[b], sy[b], tft.color565(cb / 3, cb, cb));
      lx1[i] = sx[a]; ly1[i] = sy[a]; lx2[i] = sx[b]; ly2[i] = sy[b];
      lineActive[i] = true;
    }
    lineCount = 12;
    // Draw particles
    for (int i = 0; i < NUM_PARTICLES; i++) {
      if (pd[i] > 0.4 && pd[i] < 6.0) {
        int r = 1 + (uint8_t)((1.0 / pd[i]) * 3);
        float norm = (pd[i] - 0.5) / 5.5;
        uint8_t bright = (uint8_t)((1.0 - norm) * 255);
        if (bright > 255) bright = 255;
        tft.fillCircle(px[i], py[i], r, tft.color565(bright / 3, bright / 2, bright));
        prevPx[i] = px[i]; prevPy[i] = py[i];
      } else {
        prevPx[i] = -1;
      }
    }
  }

  // ─── Direction indicator ───
  tft.fillRect(WIDTH - 20, 0, 20, 10, ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(WIDTH - 16, 1);
  tft.setTextColor(tft.color565(60, 100, 60), ST7735_BLACK);
  tft.print(dir > 0 ? ">>" : "<<");

  // ─── WS2812 ───
  uint8_t h = (uint8_t)(t * 0.04);
  uint8_t lr, lg, lb;
  uint8_t reg = h / 43, rem = (h - reg * 43) * 6;
  switch (reg) {
    case 0: lr = 255; lg = rem; lb = 0; break;
    case 1: lr = 255 - rem; lg = 255; lb = 0; break;
    case 2: lr = 0; lg = 255; lb = rem; break;
    case 3: lr = 0; lg = 255 - rem; lb = 255; break;
    case 4: lr = rem; lg = 0; lb = 255; break;
    default: lr = 255; lg = 0; lb = 255 - rem; break;
  }
  pixels.setPixelColor(0, pixels.Color(lr, lg, lb));
  pixels.show();

  delay(20);
}
