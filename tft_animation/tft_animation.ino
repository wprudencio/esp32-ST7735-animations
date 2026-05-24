#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>

// TFT pins (Software SPI)
#define TFT_CS   4
#define TFT_DC   8
#define TFT_RST  9
#define TFT_MOSI 7
#define TFT_SCLK 6

#define TOUCH_PIN 0
#define LED_PIN   10
#define NUMPIXELS 1

// Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Screen dimensions after rotation(1) = 160x128
#define WIDTH  160
#define HEIGHT 128

int mode = 0;
const int MODE_COUNT = 7;
bool last_touch = false;

// For FPS
unsigned long frameCount = 0;
unsigned long lastFPSTime = 0;
float fps = 0;

// --- Random helpers ---
uint16_t randColor() {
  return tft.color565(random(256), random(256), random(256));
}

// --- Mode 0: Rainbow bars sweeping ---
void mode_rainbow_bars() {
  static uint8_t shift = 0;
  shift += 2;
  for (int x = 0; x < WIDTH; x++) {
    uint8_t r = (x + shift) & 0xFF;
    uint8_t g = (x + shift + 85) & 0xFF;
    uint8_t b = (x + shift + 170) & 0xFF;
    uint16_t c = tft.color565(r, g, b);
    tft.drawFastVLine(x, 0, HEIGHT, c);
  }
}

// --- Mode 1: Bouncing ball ---
void mode_bouncing_ball() {
  static int bx = 80, by = 64;
  static int vx = 3, vy = 2;
  static int radius = 8;
  static uint16_t color = ST7735_RED;

  // Erase old ball
  tft.fillCircle(bx, by, radius + 1, ST7735_BLACK);

  bx += vx; by += vy;
  if (bx - radius < 0 || bx + radius >= WIDTH)  { vx = -vx; color = randColor(); }
  if (by - radius < 0 || by + radius >= HEIGHT) { vy = -vy; color = randColor(); }

  tft.fillCircle(bx, by, radius, color);
  pixels.setPixelColor(0, pixels.Color((color >> 8) & 0xFF, (color >> 16) & 0xFF, color & 0xFF));
  pixels.show();
}

// --- Mode 2: Starfield (particle) ---
#define NUM_STARS 60
void mode_starfield() {
  static struct { int16_t x, y; int8_t vx, vy; uint8_t bright; } stars[NUM_STARS];
  static bool init = false;
  if (!init) {
    for (int i = 0; i < NUM_STARS; i++) {
      stars[i].x = random(WIDTH);
      stars[i].y = random(HEIGHT);
      stars[i].vx = random(-2, 3);
      stars[i].vy = random(-2, 3);
      stars[i].bright = random(50, 255);
    }
    init = true;
  }

  tft.fillScreen(ST7735_BLACK);
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i].x += stars[i].vx;
    stars[i].y += stars[i].vy;
    if (stars[i].x < 0 || stars[i].x >= WIDTH)  stars[i].vx = -stars[i].vx;
    if (stars[i].y < 0 || stars[i].y >= HEIGHT) stars[i].vy = -stars[i].vy;
    uint8_t b = stars[i].bright;
    tft.drawPixel(stars[i].x, stars[i].y, tft.color565(b, b, b));
  }
}

// --- Mode 3: Color wave (plasma) ---
void mode_plasma() {
  static float t = 0;
  t += 0.15;
  for (int y = 0; y < HEIGHT; y += 2) {
    for (int x = 0; x < WIDTH; x += 2) {
      float v = sin(x * 0.08 + t) + sin(y * 0.06 + t) + sin((x + y) * 0.05 + t);
      v = (v + 3) / 6; // normalize to 0..1
      uint8_t r = (uint8_t)(sin(v * 6.28 + 0) * 127 + 128);
      uint8_t g = (uint8_t)(sin(v * 6.28 + 2.09) * 127 + 128);
      uint8_t b = (uint8_t)(sin(v * 6.28 + 4.19) * 127 + 128);
      tft.drawPixel(x, y, tft.color565(r, g, b));
      tft.drawPixel(x + 1, y, tft.color565(r, g, b));
      tft.drawPixel(x, y + 1, tft.color565(r, g, b));
      tft.drawPixel(x + 1, y + 1, tft.color565(r, g, b));
    }
  }
}

// --- Mode 4: Spinning wheel ---
void mode_spinner() {
  static uint16_t angle = 0;
  angle += 5;
  tft.fillScreen(ST7735_BLACK);
  for (int i = 0; i < 12; i++) {
    float a = (angle + i * 30) * 3.14159 / 180;
    int x1 = 80 + cos(a) * 40;
    int y1 = 64 + sin(a) * 40;
    int x2 = 80 + cos(a) * 55;
    int y2 = 64 + sin(a) * 55;
    tft.drawLine(x1, y1, x2, y2, tft.color565(i * 20, 255 - i * 20, 128));
  }
}

// --- Mode 5: Fill flash (speed test) ---
void mode_fill_flash() {
  static uint8_t step = 0;
  step += 2;
  uint16_t colors[] = {ST7735_RED, ST7735_GREEN, ST7735_BLUE, ST7735_YELLOW, ST7735_CYAN, ST7735_MAGENTA};
  tft.fillScreen(colors[(step / 4) % 6]);
  uint8_t c = (step / 4) % 6 == 0 ? 255 : (step / 4) % 6 == 1 ? 255 : (step / 4) % 6 == 2 ? 255 : 128;
  pixels.setPixelColor(0, pixels.Color(
    (step / 4) % 6 == 0 ? 255 : (step / 4) % 6 == 3 ? 255 : 0,
    (step / 4) % 6 == 1 ? 255 : (step / 4) % 6 == 4 ? 255 : 0,
    (step / 4) % 6 == 2 ? 255 : (step / 4) % 6 == 5 ? 255 : 0
  ));
  pixels.show();
}

// --- Mode 6: Music Spectrum Visualizer ---
#define NUM_BARS 16
void mode_spectrum() {
  static float heights[NUM_BARS];
  static float targets[NUM_BARS];
  static float phases[NUM_BARS];
  static bool init = false;
  if (!init) {
    for (int i = 0; i < NUM_BARS; i++) {
      heights[i] = 0;
      targets[i] = 0;
      phases[i] = random(100) * 0.1;
    }
    init = true;
  }

  // Erase old bars
  tft.fillRect(0, 0, WIDTH, HEIGHT - 10, ST7735_BLACK);

  // Simulate spectrum: each band has its own sine + noise
  unsigned long t = millis();
  for (int i = 0; i < NUM_BARS; i++) {
    float freq = 0.5 + i * 0.3;               // low→high frequency
    float amp   = 0.3 + sin(t * 0.002 + phases[i]) * 0.3;
    float wave  = sin(t * 0.005 * freq + phases[i]) * 0.4
                + sin(t * 0.011 * freq + phases[i] * 2) * 0.3
                + sin(t * 0.023 * freq + phases[i] * 3) * 0.2
                + ((float)random(100) / 100.0) * 0.15;

    targets[i] = (amp + wave) * 0.5;
    if (targets[i] < 0.05) targets[i] = 0;
    if (targets[i] > 1.0)  targets[i] = 1.0;

    // Attack: rise fast, fall slow (like a real spectrum analyzer)
    if (targets[i] > heights[i]) {
      heights[i] += (targets[i] - heights[i]) * 0.6;
    } else {
      heights[i] += (targets[i] - heights[i]) * 0.08;
    }

    int barH = heights[i] * (HEIGHT - 20);
    int barX = i * (WIDTH / NUM_BARS);
    int barW = (WIDTH / NUM_BARS) - 2;

    // Color gradient: low=freq (red/orange) → high=freq (blue/cyan)
    uint8_t r = (uint8_t)((1.0 - i / (float)NUM_BARS) * 255);
    uint8_t g = (uint8_t)((i < NUM_BARS / 2)
                ? (i / (float)(NUM_BARS / 2)) * 200
                : (1.0 - (i - NUM_BARS / 2) / (float)(NUM_BARS / 2)) * 200);
    uint8_t b = (uint8_t)((i / (float)NUM_BARS) * 255);

    tft.fillRect(barX, HEIGHT - 10 - barH, barW, barH, tft.color565(r, g, b));
  }

  // WS2812 pulses to the beat (simulated)
  float beat = (sin(millis() * 0.008) + 1) * 0.5;
  pixels.setPixelColor(0, pixels.Color(
    (uint8_t)(beat * 255),
    (uint8_t)((1 - beat) * 128),
    (uint8_t)(beat * 200)
  ));
  pixels.show();
}

// --- Draws the FPS counter in top-left ---
void draw_fps() {
  frameCount++;
  unsigned long now = millis();
  if (now - lastFPSTime >= 1000) {
    fps = frameCount * 1000.0 / (now - lastFPSTime);
    frameCount = 0;
    lastFPSTime = now;
  }
  tft.fillRect(0, 0, 40, 10, ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.print((int)fps);
  tft.print(" FPS");
}

// --- Mode label at bottom ---
void draw_mode_label() {
  const char *names[] = {"Rainbow Bars", "Bounce", "Starfield", "Plasma", "Spinner", "Fill Flash", "Spectrum"};
  tft.fillRect(0, HEIGHT - 10, WIDTH, 10, ST7735_BLACK);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(2, HEIGHT - 9);
  tft.print("[");
  tft.print(mode + 1);
  tft.print("/");
  tft.print(MODE_COUNT);
  tft.print("] ");
  tft.print(names[mode]);
}

void setup() {
  pinMode(TOUCH_PIN, INPUT);

  pixels.begin();
  pixels.setBrightness(40);
  pixels.clear();
  pixels.show();

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);

  lastFPSTime = millis();
}

void loop() {
  // Touch to advance mode
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);
  if (touched && !last_touch) {
    mode = (mode + 1) % MODE_COUNT;
    delay(200); // debounce
    tft.fillScreen(ST7735_BLACK);
  }
  last_touch = touched;

  // Run current mode
  switch (mode) {
    case 0: mode_rainbow_bars(); break;
    case 1: mode_bouncing_ball(); break;
    case 2: mode_starfield(); break;
    case 3: mode_plasma(); break;
    case 4: mode_spinner(); break;
    case 5: mode_fill_flash(); break;
    case 6: mode_spectrum(); break;
  }

  // Overlays
  draw_fps();
  draw_mode_label();

  // Small delay to keep visible & readable
  delay(16); // ~60fps target
}
