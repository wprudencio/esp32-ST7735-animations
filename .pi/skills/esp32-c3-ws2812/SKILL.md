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
| `3d_cube/` | 3D wireframe cube + 3 colorful bouncing balls — rainbow edges, HSV-cycling balls with specular highlights, framebuffer rendering, touch to reverse spin |
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

### ⚠️ Incremental = flicker on complex scenes

The incremental method (erase old, draw new) works for simple bar charts or few elements. But with many moving elements, each `fillCircle`/`drawLine` goes through a **separate SPI transaction** with command overhead. The display updates pixel-by-pixel as data arrives, so the user sees:

```
[erase element 1] → [black patch visible] → [erase element 2] → ...
→ [draw element 1] → [element appears] → [draw element 2] → ...
```

This sub-frame visibility creates **flicker/blinking** that can't be eliminated by rearranging erase/draw order.

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

## 4. The Framebuffer Method — Zero-Flicker Rendering (Recommended for 3D/Complex Scenes)

### Why framebuffer?

The ST7735 has **no double-buffer** — every SPI write updates the screen immediately. For smooth 3D animation, the only way to avoid visible sub-frame updates is to:

1. **Draw the entire frame in RAM** (a 40KB framebuffer)
2. **Blast the complete frame to the display in ONE SPI transaction**

This eliminates ALL flicker. The screen transitions atomically from old frame to new frame.

### Memory Budget

```cpp
// 160×128 pixels × 2 bytes (RGB565) = 40,960 bytes ≈ 40KB
uint16_t fb[WIDTH * HEIGHT];  // static allocation — no malloc!
```

ESP32-C3 has 320KB total RAM. 40KB framebuffer + ~15KB globals = 55KB used, ~265KB free. **Easily fits.** Always use `static uint16_t fb[...]` (global scope) to avoid stack/heap fragmentation.

### Pixel Set Macro (with bounds check)

```cpp
#define FPIX(x,y,c) if((x)>=0&&(x)<WIDTH&&(y)>=0&&(y)<HEIGHT) fb[(y)*WIDTH+(x)]=(c)
```

### Drawing Functions (all in-memory, no SPI)

**Clear framebuffer:**
```cpp
void fbClear() {
  for (int i=0; i<WIDTH*HEIGHT; i++) fb[i] = ST7735_BLACK;
}
```

**Horizontal line (optimized for fillCircle):**
```cpp
void fbHLine(int x, int y, int w, uint16_t c) {
  if (y<0||y>=HEIGHT) return;
  if (x<0) { w+=x; x=0; }
  if (x+w>WIDTH) w=WIDTH-x;
  if (w<=0) return;
  uint16_t* p = &fb[y*WIDTH+x];
  while (w--) *p++ = c;        // pointer fill = fast
}
```

**Filled circle (midpoint algorithm, uses fbHLine for each scanline):**
```cpp
void fbFillCircle(int cx, int cy, int r, uint16_t c) {
  int x=0, y=r, d=3-2*r;
  while (x<=y) {
    fbHLine(cx-x, cy-y, 2*x+1, c);  // top segment
    fbHLine(cx-x, cy+y, 2*x+1, c);  // bottom segment
    fbHLine(cx-y, cy-x, 2*y+1, c);  // left segment
    fbHLine(cx-y, cy+x, 2*y+1, c);  // right segment
    if (d<0) d+=4*x+6;
    else { d+=4*(x-y)+10; y--; }
    x++;
  }
}
```

**Circle outline (Bresenham, 8-way symmetry):**
```cpp
void fbDrawCircle(int cx, int cy, int r, uint16_t c) {
  int x=0, y=r, d=3-2*r;
  while (x<=y) {
    FPIX(cx+x,cy+y,c); FPIX(cx-x,cy+y,c);
    FPIX(cx+x,cy-y,c); FPIX(cx-x,cy-y,c);
    FPIX(cx+y,cy+x,c); FPIX(cx-y,cy+x,c);
    FPIX(cx+y,cy-x,c); FPIX(cx-y,cy-x,c);
    if (d<0) d+=4*x+6;
    else { d+=4*(x-y)+10; y--; }
    x++;
  }
}
```

**Line (Bresenham):**
```cpp
void fbDrawLine(int x0, int y0, int x1, int y1, uint16_t c) {
  int dx=abs(x1-x0), sx=x0<x1?1:-1;
  int dy=-abs(y1-y0), sy=y0<y1?1:-1;
  int err=dx+dy, e2;
  for (;;) {
    FPIX(x0,y0,c);
    if (x0==x1 && y0==y1) break;
    e2=2*err;
    if (e2>=dy) { err+=dy; x0+=sx; }
    if (e2<=dx) { err+=dx; y0+=sy; }
  }
}
```

### Flush to Display (the magic)

```cpp
void fbFlush() {
  tft.startWrite();
  tft.setAddrWindow(0, 0, WIDTH, HEIGHT);   // one address window
  tft.writePixels(fb, WIDTH*HEIGHT);         // one 40KB SPI burst
  tft.endWrite();
}
```

If `writePixels` is not available, use `tft.drawRGBBitmap(0, 0, fb, WIDTH, HEIGHT)` instead.

### Complete Render Loop Pattern

```cpp
void loop() {
  // 1. Physics / logic (fast, in-memory)
  updatePhysics();
  
  // 2. Clear framebuffer
  fbClear();
  
  // 3. Draw everything to framebuffer (all in-memory, zero SPI)
  drawScene();  // fbDrawLine(), fbFillCircle(), fpDrawCircle(), etc.
  
  // 4. One atomic SPI burst to display
  fbFlush();
  
  delay(8);  // ~100 FPS target (8ms render + 8ms delay = 16ms)
}
```

### When to Use Framebuffer vs Incremental

| Scenario | Method | Why |
|---|---|---|
| Bar charts, simple meters | Incremental | Few elements, minimal erase cost |
| 3D wireframes, rotating objects | **Framebuffer** | Many moving lines, sub-frame flicker unavoidable otherwise |
| Particle systems (>5 elements) | **Framebuffer** | Erase-then-draw gap too large |
| Static text + occasional updates | Incremental | No continuous movement |
| Games, animations, smooth motion | **Framebuffer** | Zero-flicker requirement |

### Performance: Framebuffer vs Direct SPI

| Operation | Direct SPI (each op) | Framebuffer (in RAM) |
|---|---|---|
| `fillCircle(r=6)` | ~1ms (SPI command + data) | ~10μs (pointer writes) |
| `drawLine(40px)` | ~0.2ms | ~5μs |
| Full frame erase | ~11ms (fillScreen = 40KB SPI) | ~1ms (memset 40KB in SRAM) |
| Frame flush | N/A | ~8ms (one 40KB SPI burst at 40MHz) |

RAM drawing is **100-1000x faster** than SPI. The only SPI cost is the single flush at the end. For complex scenes with 50+ draw operations, the framebuffer approach is dramatically faster AND flicker-free.

### Thick Lines in Framebuffer

To draw thick edges (2-3px), draw multiple offset lines — free in framebuffer since there's no per-line SPI cost:

```cpp
// 3px-wide edge
fbDrawLine(sx[a],   sy[a], sx[b],   sy[b], edgeColor);
fbDrawLine(sx[a]+1, sy[a], sx[b]+1, sy[b], edgeColor);
fbDrawLine(sx[a]-1, sy[a], sx[b]-1, sy[b], dimColor);
```

### Layered Blob Rendering (Soft Glow Effect)

For 3D spheres/particles, layer multiple circles to create depth and specular highlights — cheap in framebuffer:

```cpp
// 4-layer ball: halo → body → core → specular highlight
fbDrawCircle(px, py, vr+1, darkHalo);      // outer ring
fbFillCircle(px, py, vr,   dimBody);        // soft body
fbFillCircle(px, py, vr-1, brightCore);     // bright core
fbFillCircle(px-1, py-1, vr-3, white);      // specular dot
```

### Memory Warning

- **Don't put framebuffer on the stack** — `uint16_t fb[20480]` as a local variable will overflow the stack. Always declare at **global scope**.
- Framebuffer is `static` — initialized once, reused every frame.
- Total RAM: 40KB fb + ~15KB libs + ~5KB variables = ~60KB. ESP32-C3 has 320KB. Plenty of headroom.

---

## 5. 3D Rendering Patterns

## 5. 3D Rendering Patterns

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

## 6. Performance Optimization (General)

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

## 7. Common Pitfalls

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

## 8. Performance Checklist

- [ ] `SPI.begin()` called with correct pins
- [ ] **For 3D/complex scenes: use framebuffer** (not incremental SPI)
- [ ] Framebuffer declared at **global scope** (not stack)
- [ ] Only one `fbFlush()` call per frame (one SPI burst)
- [ ] `delay()` is ≤ 16ms for 60 FPS, ≤ 8ms for 100+ FPS
- [ ] For incremental (simple scenes): each element tracks its previous state
- [ ] For incremental: only changed pixels are written each frame
- [ ] First frame handled separately as full draw (both methods)
- [ ] Pre-compute rotation matrices for 3D (6 trig calls vs per-vertex)
- [ ] Colors pre-computed (not recalculated per frame if constant)
- [ ] Physics in local space for container-based simulations
- [ ] Use `fbHLine` for fillCircle — not per-pixel `FPIX` in a loop

---

## Important Notes

- The ESP32-C3's built-in USB-Serial/JTAG handles both flashing and serial — no external UART needed
- GPIO 10 is safe to use for WS2812 (no strapping conflicts on C3)
- Set LED brightness conservatively (`setBrightness(50)`) — full brightness can draw too much current from 3.3V
- If flashing fails, hold **BOOT** button (GPIO 9) while connecting
- Always run `arduino-cli board list` to confirm the port — it changes on reconnect
- All paths in commands are relative to the project root — no absolute paths needed
