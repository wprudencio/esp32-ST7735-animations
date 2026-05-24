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

## Important Notes

- The ESP32-C3's built-in USB-Serial/JTAG handles both flashing and serial — no external UART needed
- GPIO 10 is safe to use for WS2812 (no strapping conflicts on C3)
- Set LED brightness conservatively (`setBrightness(50)`) — full brightness can draw too much current from the 3.3V pin
- If flashing fails, hold the **BOOT** button (GPIO 9) while connecting, or press it during the `Connecting...` phase
