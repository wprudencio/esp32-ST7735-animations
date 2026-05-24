---
name: esp32-c3-ws2812
description: Flash and develop for the ESP32-C3 board with a WS2812 (NeoPixel) LED on GPIO 10 using Arduino CLI. Use when working with the esp32 project folder, ESP32-C3, WS2812 LEDs, or the blue blink sketch.
---

# ESP32-C3 + WS2812 on GPIO 10

## Hardware

| Property | Value |
|---|---|
| **Chip** | ESP32-C3 (RISC-V, single core @ 160MHz) |
| **Port** | `/dev/cu.usbmodemxxxxx` (check with `arduino-cli board list`) |
| **Flash** | 4MB embedded |
| **LED** | WS2812 (NeoPixel) on **GPIO 10** |
| **Tools** | Arduino CLI (`arduino-cli` must be in PATH or at project root) |

## Quick Start — Blue Blink

```bash
cd <project-root>
export PATH="$PWD/bin:$PATH"  # if arduino-cli is in bin/

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32c3 ws2812_blue_blink

# Flash (replace port with your device)
arduino-cli upload -p /dev/cu.usbmodemXXXXX --fqbn esp32:esp32:esp32c3 ws2812_blue_blink
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
  pixels.setPixelColor(0, pixels.Color(255, 0, 0));
  pixels.show();
  delay(500);
  pixels.clear();
  pixels.show();
  delay(500);
}
EOF

arduino-cli compile --fqbn esp32:esp32:esp32c3 my_sketch
arduino-cli upload -p /dev/cu.usbmodemXXXXX --fqbn esp32:esp32:esp32c3 my_sketch
```

## Monitoring Serial Output

```bash
arduino-cli monitor -p /dev/cu.usbmodemXXXXX -c baudrate=115200
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
| `spectrum_psy/` | Psychedelic mirrored spectrum — 24 bars (12L+12R) radiating from center, rainbow cycling, center flare, rainbow WS2812 |
| `3d_cube/` | 3D wireframe cube with liquid particles — rainbow edges, local-space physics, pre-computed rotation matrix, touch to reverse spin |
| `dvd_bounce/` | DVD screensaver — bitmap-traced logo from SVG, bounces diagonally, 9 colors, touch changes direction |
| `space_game/` | 3D space dodge game — parallax starfield, asteroid shooting, lives/score, touch to thrust, game over + restart |
| `3d_spectrum/` | 3D box-art spectrum visualizer — extruded bars with front/top/side faces, 5 color palettes, touch to cycle palettes |

---

# Display Animation Guide — ST7735 on ESP32-C3

## 1. Hardware SPI Initialization (Critical!)

The ST7735 communicates over SPI. On the ESP32-C3 you **must** call `SPI.begin()` with the correct pins before `tft.initR()`:

```cpp
// Example pinout — adjust to your wiring
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    3
#define TFT_MOSI  2
#define TFT_SCLK  1

// Hardware SPI constructor
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  // ⚠️ THIS IS MANDATORY — tells the ESP32-C3 which pins are MOSI/SCLK
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);     // 0/2 = portrait, 1/3 = landscape
  tft.fillScreen(ST7735_BLACK);
}
```

**Without `SPI.begin()`, the display gets no data → white screen.**

## 2. The Rendering Pipeline — How the Display Works

Every `fillRect()`, `drawPixel()`, etc. goes through this chain:

```
CPU → SPI.transfer(data) → MOSI pin → Display controller → Frame buffer → LCD
```

Each `fillRect()` sends:
- A **command** to set the drawing window (x, y, w, h)
- A **data burst** of RGB565 pixels (2 bytes per pixel)

**Minimizing SPI writes is critical** for smooth animation.

## 3. Animation Strategy — The Incremental Method

### ❌ Don't full-clear every frame
```cpp
tft.fillScreen(ST7735_BLACK);   // visible flash
for (...) { tft.fillRect(...); } // sequential draw
```

### ❌ Don't two-pass (black then color)
```cpp
for (...) { tft.fillRect(bx, 0, bw, HEIGHT, BLACK); }  // all black flash
for (...) { tft.fillRect(bx, top, bw, h, COLOR); }     // then color
```

### ✅ Only redraw the pixels that changed
```cpp
if (newTop < prevTop) {
  // Bar grew → colored sliver at new top
  tft.fillRect(bx, newTop, bw, prevTop - newTop, barColor);
} else if (newTop > prevTop) {
  // Bar shrank → black sliver over exposed area
  tft.fillRect(bx, prevTop, bw, newTop - prevTop, BLACK);
}
// Same height = zero SPI writes
```

### The "First Frame" Pattern
```cpp
bool firstFrame = true;

void loop() {
  if (firstFrame) {
    tft.fillScreen(ST7735_BLACK);   // full draw on frame 1
    // ... draw everything ...
    firstFrame = false;
  } else {
    // Incremental updates from frame 2+
  }
}
```

## 4. Performance Optimization

### Pre-compute rotation matrices
```cpp
// ❌ Per-particle trig (slow):
void rotateParticle(Vec3 *p, float ax, float ay, float az) {
  float cx=cosf(ax), sx=sinf(ax); // called for every particle
  ...
}

// ✅ Pre-compute matrix once per frame:
float rot[9];  // 3x3 matrix
void compRot(float ax, float ay, float az) {
  // 6 trig calls total, then reuse for all particles
  rot[0] = cy*cz + sx*sy*sz; ... 
}
void applyRot(Vec3 *p) {
  // 9 multiplies + 6 adds — no trig!
  p->x = p->x*rot[0] + p->y*rot[1] + p->z*rot[2]; ...
}
```

### drawPixel vs fillCircle
- `drawPixel` = 1 SPI command + 2 bytes = ~5μs
- `fillCircle(r=3)` = ~28 pixels = ~140μs
- For many particles: use `drawPixel`. For few elements (<5): `fillCircle` is fine.

### Local-space physics for 3D containers
```cpp
// ✅ Physics in LOCAL space = easy bounds checking
float bound = 0.85;
pos[i].y -= 0.002f;        // gravity
if (pos[i].y < -bound) {   // trivial collision
  pos[i].y = -bound;
  vel[i].y *= -0.1f;       // liquid: no bounce
}

// Then rotate to world space ONCE for rendering:
Vec3 world = pos[i];
applyRot(&world);
project(world, &sx, &sy, &depth);
```

## 5. Common Pitfalls

| Problem | Cause | Fix |
|---|---|---|
| **White screen** | `SPI.begin()` not called | Add `SPI.begin(SCLK, -1, MOSI, CS)` before `tft.initR()` |
| **Tearing / flash** | Full-screen clear every frame | Use incremental updates |
| **Bars flicker** | Erase-then-draw per element | Batch operations or diff-only |
| **Slow FPS** | `delay()` too long, too many SPI writes | Shorten delay to 16-30ms, minimize pixels |
| **Display won't init** | Wrong `initR` parameter | Try `INITR_BLACKTAB`, `INITR_GREENTAB`, `INITR_144GREENTAB` |
| **Ghosting** | Drawing outside visible area | Verify `setRotation()` matches orientation |
| **Port not found** | Device disconnected or reconnected | Run `arduino-cli board list` to find new port |
| **Flash fails** | Boot mode issue | Hold BOOT button (GPIO 9) during connecting phase |

## 6. Performance Checklist

- [ ] `SPI.begin()` called with correct pins
- [ ] No `fillScreen()` or full `fillRect(0, 0, W, H)` in hot loop
- [ ] Each element tracks its previous state
- [ ] Only changed pixels are written each frame
- [ ] `delay()` is ≤ 30ms (or use millis() delta)
- [ ] Colors pre-computed (not recalculated per frame if constant)
- [ ] First frame handled separately as full draw
- [ ] Pre-compute rotation matrices for 3D (6 trig calls vs 75)
- [ ] Use `drawPixel` over `fillCircle` for many particles
- [ ] Physics in local space for container-based simulations

---

## Important Notes

- The ESP32-C3's built-in USB-Serial/JTAG handles both flashing and serial — no external UART needed
- GPIO 10 is safe to use for WS2812 (no strapping conflicts on C3)
- Set LED brightness conservatively (`setBrightness(50)`) — full brightness can draw too much current from 3.3V
- If flashing fails, hold **BOOT** button (GPIO 9) while connecting
- Always run `arduino-cli board list` to confirm the port — it changes on reconnect
- All paths in commands are relative to the project root — no absolute paths needed
