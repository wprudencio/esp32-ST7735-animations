# ESP32-C3 Projects

ESP32-C3 RISC-V board with ST7735 160×128 TFT display (SPI), WS2812 NeoPixel LED (GPIO 10), and button (GPIO 0 / BOOT).

## Hardware

| GPIO | Role |
|------|------|
| 1 | TFT SCLK |
| 2 | TFT MOSI |
| 3 | TFT DC |
| 4 | TFT RST |
| 5 | TFT CS |
| 10 | WS2812 NeoPixel data |
| 0 | Button (BOOT) |

**Chip:** ESP32-C3 (RISC-V, 160 MHz, 4MB flash)
**Display:** ST7735 160×128, landscape (rotation 1)
**⚠️** Always call `SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS)` before `tft.initR()` or the display stays white.

## Setup

```bash
# Add arduino-cli to PATH
export PATH="$PWD/bin:$PATH"

# Compile & flash any sketch
arduino-cli compile --fqbn esp32:esp32:esp32c3 <sketch_dir>
arduino-cli upload -p /dev/cu.usbmodemXXXXX --fqbn esp32:esp32:esp32c3 <sketch_dir>

# Find your port
arduino-cli board list

# Monitor serial
arduino-cli monitor -p /dev/cu.usbmodemXXXXX -c baudrate=115200
```

## Sketches

| Sketch | Display | Touch | LED | Description |
|--------|---------|-------|-----|-------------|
| [`flappy_bird`](flappy_bird/) | ✅ | ✅ | ✅ | Flappy Bird clone, Game Boy DMG green palette, minimalist pixel art |
| [`3d_cube`](3d_cube/) | ✅ | ✅ | ✅ | Wireframe cube + up to 500 liquid particles with collision physics |
| [`spectrum`](spectrum/) | ✅ | — | ✅ | 16-bar audio spectrum visualizer, breathing color gradient |

### Flappy Bird

Classic side-scrolling obstacle game in Game Boy green (4-shade DMG palette).

- **Press BOOT** to flap / start / retry
- Green LED flash on flap, red pulse on death, idle glow in menu
- Difficulty ramps as pipes spawn faster
- High score tracked during session
- Clean minimalist design: flat sky, solid ground, crisp pixel pipes

### 3D Cube

Wireframe cube with bouncing particles contained inside. Each tap adds a new ball (up to 500). Particles have mass-based physics, collision response, and rainbow color cycling. Dynamic camera orbits with breathing zoom.

### Spectrum

Music-reactive bar visualizer. 16 bars animate with multi-layered sine waves + noise. Incremental rendering — only changed pixels update per frame. LED pulses to the beat.

## Flashing Tips

- Hold **BOOT** (GPIO 9) if upload fails to connect
- Port changes on reconnect — always run `arduino-cli board list` first
- Keep `setBrightness(≤40)` — full brightness draws too much from 3.3V
- Use **framebuffer** for smooth animation: draw everything in RAM, flush in one SPI burst
