# ESP32-C3 Projects

ESP32-C3 + ST7735 TFT (SPI) + WS2812 (GPIO 10) + TTP223 touch (GPIO 0).

## Pinout

| GPIO | Role |
|------|------|
| 1 | TFT_SCLK |
| 2 | TFT_MOSI |
| 3 | TFT_DC |
| 4 | TFT_RST |
| 5 | TFT_CS |
| 10 | WS2812 data |
| 0 | TTP223 touch (optional) |

All sketches call `SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS)` or they white-screen.

## Setup

```bash
export PATH="$PWD/bin:$PATH"
arduino-cli compile --fqbn esp32:esp32:esp32c3 <sketch>
arduino-cli upload -p /dev/cu.usbmodemXXXXX --fqbn esp32:esp32:esp32c3 <sketch>
```

## Sketches

| Sketch | Disp | Touch | LED | What it does |
|--------|------|-------|-----|-------------|
| `3d_cube` | ✅ | ✅ | ✅ | wireframe cube + liquid particles |
| `spectrum` | ✅ | — | ✅ | 16-bar music viz |

## Flashing

- Hold BOOT (GPIO 9) if upload fails
- Port changes on reconnect — run `arduino-cli board list` first
- Keep `setBrightness(40)` to avoid drawing too much from 3.3V
