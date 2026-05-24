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
| `hello_tft/` | ST7735 TFT (1.8") + TTP223 touch → WS2812 — "Hello World" on screen, touch state displayed, WS2812 glows blue on touch |
| `tft_animation/` | 6 animation modes (rainbow bars, bounce, starfield, plasma, spinner, fill flash) with FPS counter. Tap touch to cycle modes |
| `spectrum/` | Music spectrum visualizer — 16 bars with simulated audio, fast-attack/slow-decay, WS2812 pulse |

### Touch LED Logic

`touch_led/touch_led.ino` reads a TTP223 capacitive touch module on GPIO 0. When touched (output goes HIGH), the WS2812 on GPIO 10 glows blue. When released, it turns off.

**Wiring:**
- TTP223 VCC → 3.3V
- TTP223 GND → GND
- TTP223 OUT → GPIO 0

### Hello TFT

`hello_tft/hello_tft.ino` combines the TFT display, touch sensor, and WS2812:

| Component | Pins |
|---|---|
| **ST7735 TFT** | CS=5, RST=4, DC=3, MOSI=2, SCLK=1 (Software SPI) |
| **TTP223 Touch** | OUT → GPIO 0 |
| **WS2812 LED** | GPIO 10 |

**Behavior:**
- Shows "Hello World!" on the TFT in cyan
- Reads touch from GPIO 0 and displays "TOUCHED" (green) or "released" (red)
- Lights the WS2812 blue when touched, off when released
- Display only updates on state change (no flicker)

## Animation Strategy — Smooth & Fast on ST7735

The key insight for tear-free animation on slow-SPI displays like the ST7735:

### ❌ Don't full-clear every frame
```cpp
// BAD — visible tearing/flash
tft.fillScreen(ST7735_BLACK);  // clears everything
for (...) { tft.fillRect(...); }  // redraws everything
```
You see the brief black flash before the new frame appears.

### ❌ Don't two-pass (black then color)
```cpp
// BAD — double the visual passes
for (...) { tft.fillRect(bx, 0, bw, HEIGHT, BLACK); }  // all black
for (...) { tft.fillRect(bx, top, bw, h, COLOR); }     // all color
```
You see black columns flash before colors appear.

### ✅ Only redraw the pixels that changed
```cpp
// GOOD — only the moving edge, nothing else
if (newTop < prevTop) {
  // Bar grew → draw colored sliver at new top
  tft.fillRect(bx, newTop, bw, prevTop - newTop, barColor);
} else if (newTop > prevTop) {
  // Bar shrank → erase exposed area at top
  tft.fillRect(bx, prevTop, bw, newTop - prevTop, BLACK);
}
// Same height = zero SPI writes
```

The essence: **track the previous state of every element, and only write the pixels that differ.** For a spectrum analyzer, only the bar tops move — the bottom 90% of each bar stays identical frame to frame, so don't touch it.

### General Rules for Smooth Display Animation

1. **Never full-clear** — always track previous positions
2. **Write only the changed pixels** — the smaller the rect, the faster
3. **Compute before draw** — calculate all positions/colors first, then batch the SPI writes if possible
4. **Same height = skip** — if an element didn't change, don't write a single byte
5. **Minimize delay()** — use short delays (16-30ms) for ~30-60fps

## Important Notes

- The ESP32-C3's built-in USB-Serial/JTAG handles both flashing and serial — no external UART needed
- GPIO 10 is safe to use for WS2812 (no strapping conflicts on C3)
- Set LED brightness conservatively (`setBrightness(50)`) — full brightness can draw too much current from the 3.3V pin
- If flashing fails, hold the **BOOT** button (GPIO 9) while connecting, or press it during the `Connecting...` phase
