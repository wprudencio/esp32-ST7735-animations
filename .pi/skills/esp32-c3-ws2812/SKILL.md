---
name: esp32-c3-ws2812
description: Flash and develop for the ESP32-C3 board with a WS2812 (NeoPixel) LED on GPIO 10 using Arduino CLI. Use when working with the esp32 project folder, ESP32-C3, WS2812 LEDs, or the blue blink sketch.
---

# ESP32-C3 + WS2812 on GPIO 10

## Hardware

| Property | Value |
|---|---|
| **Chip** | ESP32-C3 (RISC-V, single core @ 160MHz) |
| **Port** | `/dev/cu.usbmodem14101` |
| **Flash** | 4MB embedded |
| **LED** | WS2812 (NeoPixel) on **GPIO 10** |
| **Arduino CLI** | `/Users/weslei/personalprojects/esp32/bin/arduino-cli` |

## Quick Start — Blue Blink

```bash
cd /Users/weslei/personalprojects/esp32
export PATH="$PWD/bin:$PATH"

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32c3 ws2812_blue_blink

# Flash
arduino-cli upload -p /dev/cu.usbmodem14101 --fqbn esp32:esp32:esp32c3 ws2812_blue_blink
```

## Creating a New Sketch

```bash
mkdir -p my_sketch
cat > my_sketch/my_sketch.ino << 'EOF'
#include <Adafruit_NeoPixel.h>

#define PIN        10
#define NUMPIXELS  1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  pixels.begin();
  pixels.setBrightness(50);
}

void loop() {
  pixels.setPixelColor(0, pixels.Color(255, 0, 0));  // Red
  pixels.show();
  delay(500);
  pixels.clear();
  pixels.show();
  delay(500);
}
EOF

arduino-cli compile --fqbn esp32:esp32:esp32c3 my_sketch
arduino-cli upload -p /dev/cu.usbmodem14101 --fqbn esp32:esp32:esp32c3 my_sketch
```

## Monitoring Serial Output

```bash
arduino-cli monitor -p /dev/cu.usbmodem14101 -c baudrate=115200
```

## Board Info

```bash
arduino-cli board list
```

## Existing Sketches

| Sketch | Description |
|---|---|
| `ws2812_blue_blink/` | Simple blue blink every 1s — hello world test |
| `touch_led/` | TTP223 touch sensor on **GPIO 0** → WS2812 blue on touch |
| `hello_tft/` | ST7735 TFT + TTP223 touch → WS2812 — "Hello World" on screen, touch state displayed, WS2812 glows blue on touch |
| `tft_animation/` | 6 animation modes (rainbow bars, bounce, starfield, plasma, spinner, fill flash) with FPS counter. Tap touch to cycle modes |
| `spectrum/` | Music spectrum visualizer — 16 bars, simulated audio, fast-attack/slow-decay, WS2812 pulse to beat |

---

# Display Animation Guide — ST7735 on ESP32-C3

## 1. Hardware SPI Initialization (Critical!)

The ST7735 communicates over SPI. On the ESP32-C3 you **must** call `SPI.begin()` with the correct pins before `tft.initR()`:

```cpp
// Pins used in this project
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    3
#define TFT_MOSI  2
#define TFT_SCLK  1

// Hardware SPI constructor (uses the default SPI peripheral)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  // ⚠️ THIS LINE IS MANDATORY — tells the ESP32-C3 which pins are MOSI/SCLK
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  //              SCLK  MISO   MOSI      CS (optional)

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);     // 0/2 = portrait 128x160, 1/3 = landscape 160x128
  tft.fillScreen(ST7735_BLACK);
}
```

**Without `SPI.begin()`, the display gets no data → white screen.**

## 2. The Rendering Pipeline — How the Display Works

Every `fillRect()`, `drawPixel()`, etc. goes through this chain:

```
CPU → SPI.transfer(data) → MOSI pin → Display controller → Frame buffer → LCD
```

Each `fillRect()` command sends:
- A **command** to set the drawing window (x, y, w, h)  
- A **data burst** of RGB565 pixels (2 bytes per pixel)

**That's why minimizing SPI writes is critical.** The display's internal frame buffer refresh + the slow SPI clock (~20-40MHz, shared overhead per transaction) means every unnecessary pixel write steals time from the next frame.

## 3. Animation Strategy — The Incremental Method

### The Problem

```cpp
// ❌ Visible tearing: full screen goes black before every frame
tft.fillScreen(BLACK);
for (int i = 0; i < 16; i++) {
  tft.fillRect(x, top, w, h, color);  // bars appear one by one
}
```

This sends data for **all 20,480 pixels** every frame, plus you see the black flash.

```cpp
// ❌ Two-pass: first all black, then all color
for (int i = 0; i < 16; i++) {
  tft.fillRect(x, 0, w, HEIGHT, BLACK);  // columns flash black
}
for (int i = 0; i < 16; i++) {
  tft.fillRect(x, top, w, h, color);     // then color
}
```

You see black columns appear before color.

### The Solution — Track and Diff

Each animated element has **properties that change** (position, size) and **properties that stay constant** (color, shape). Only write the difference.

**Spectrum visualizer example:**
- Each bar's **color is fixed** (depends only on its position left→right)
- Only the **bar height changes** each frame
- The bottom portion of the bar is identical → don't redraw it

```cpp
// Pattern:
// 1. Track previous state
int prevTop[NUM_BARS];
int prevH[NUM_BARS];

// 2. Each frame, compute new state
int newH = heights[i] * (HEIGHT - 10);
int newTop = HEIGHT - newH;

// 3. Only write the difference
if (newTop < prevTop[i]) {
  // Bar grew → colored sliver at the new top
  tft.fillRect(bx, newTop, bw, prevTop[i] - newTop, barColor);
} else if (newTop > prevTop[i]) {
  // Bar shrank → black sliver over exposed area
  tft.fillRect(bx, prevTop[i], bw, newTop - prevTop[i], ST7735_BLACK);
}
// Same height → nothing to do, zero SPI writes

prevTop[i] = newTop;
```

**Result:** Instead of 20,480 pixels per frame, you write ~50-200 pixels. The display barely has to work.

### The "First Frame" Problem

The first frame has no previous state — all bars go from 0 height to their initial height. Handle it:

```cpp
bool firstFrame = true;

void loop() {
  if (firstFrame) {
    // Full redraw on frame 1
    tft.fillRect(bx, 0, bw, HEIGHT - 10, BLACK);
    tft.fillRect(bx, newTop, bw, newH, color);
    firstFrame = false;
  } else {
    // Incremental update from frame 2 onwards
    // ... diff logic ...
  }
}
```

## 4. Template for New Animation Projects

```cpp
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    3
#define TFT_MOSI  2
#define TFT_SCLK  1

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define WIDTH  160
#define HEIGHT 128

// --- Track previous state here ---
// int prevX, prevY, prevH, etc.

// --- Your elements here ---
// struct Element { int x, y, h; uint16_t color; } elements[N];

bool firstFrame = true;

void setup() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);
}

void loop() {
  // 1️⃣ Compute new state (no drawing)
  //    - Update positions, sizes based on time/math/input

  // 2️⃣ First frame: full draw
  if (firstFrame) {
    tft.fillScreen(ST7735_BLACK);
    // ... draw everything from scratch ...
    firstFrame = false;
    return;
  }

  // 3️⃣ Subsequent frames: only differences
  //    - If element moved: erase old position, draw new position
  //    - If element changed size: update the changing edge only
  //    - If element is static: skip it

  delay(16);  // ~60fps
}
```

## 5. Common Pitfalls

| Problem | Cause | Fix |
|---|---|---|
| **White screen** | `SPI.begin()` not called, or wrong pins | Add `SPI.begin(SCLK, -1, MOSI, CS)` before `tft.initR()` |
| **Tearing / flash** | Full-screen clear every frame | Use incremental updates instead |
| **Bars flicker** | Erase-then-draw per element (sequential visible updates) | Batch operations: compute all, then draw, or use diff-only |
| **Slow FPS** | `delay()` too long, or too many SPI writes | Shorten delay to 16-30ms, minimize pixel writes |
| **Display won't initialize** | Wrong `initR` parameter | Try `INITR_BLACKTAB`, `INITR_GREENTAB`, or `INITR_144GREENTAB` for different ST7735 variants |
| **Ghosting / artifacts** | Drawing outside visible area or wrong rotation | Verify `setRotation()` matches your physical orientation |

## 6. How the Spectrum Visualizer Works (Full Walkthrough)

1. **16 bars**, each 9px wide with 1px gap
2. Each bar has a **fixed color** based on its position: warm (red/orange) on left → cool (blue/cyan) on right
3. Each bar simulates an audio frequency band using **three sine waves + noise** per band, each with different frequencies to prevent any two bars from moving identically
4. **Fast attack / slow decay**: if the new target is higher, jump toward it quickly (0.6 factor); if lower, drift down slowly (0.08 factor) — mimics real hardware spectrum analyzers
5. **Only the bar top** (the changing edge) is redrawn each frame
6. The **WS2812 LED** pulses to a simulated beat (a single sine wave at ~8Hz)

This all runs at **~30fps** with barely any visible artifacts.

## 7. Performance Optimization Checklist

- [ ] `SPI.begin()` called with correct pins
- [ ] No `fillScreen()` or `fillRect(0, 0, W, H)` in the hot loop
- [ ] Each element tracks its previous state
- [ ] Only changed pixels are written
- [ ] `delay()` is ≤ 30ms (or use millis() delta)
- [ ] Colors are pre-computed (not recalculated per frame if constant)
- [ ] First frame handled separately as a full draw

---

## Important Notes

- The ESP32-C3's built-in USB-Serial/JTAG handles both flashing and serial — no external UART needed
- GPIO 10 is safe to use for WS2812 (no strapping conflicts on C3)
- Set LED brightness conservatively (`setBrightness(50)`) — full brightness can draw too much current from the 3.3V pin
- If flashing fails, hold the **BOOT** button (GPIO 9) while connecting, or press it during the `Connecting...` phase
- Always run `arduino-cli board list` to confirm the port — it changes when you reconnect
