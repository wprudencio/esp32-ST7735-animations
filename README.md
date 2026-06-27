# ESP32-C3 Projects

<video src="assets/shorts_hq.mp4" controls muted loop width="360"></video>

ESP32-C3 RISC-V board with ST7735 160×128 TFT display (SPI), WS2812 NeoPixel LED (GPIO 10), and button (GPIO 0 / BOOT).


## Development Skill

This repo includes an [Agent Skills](https://agentskills.io)-compatible skill at [`.agents/skills/esp32-c3-ws2812/`](.agents/skills/esp32-c3-ws2812/) that teaches AI coding agents how to work with this hardware:

- ESP32-C3 pinout, flashing workflow, and common pitfalls
- WS2812 NeoPixel setup on GPIO 10
- ST7735 TFT display — SPI init, incremental vs framebuffer rendering, 3D patterns
- Performance optimization and troubleshooting

The skill is auto-discovered by pi when working in this project, and is also symlinked to `~/.agents/skills/` for use by other Agent Skills–compatible agents (Claude Code, Codex, etc.) from any directory.

## Sketches

| Sketch | Display | Touch | LED | Description |
|--------|---------|-------|-----|-------------|
| [`3d_cube`](3d_cube/) | ✅ | ✅ | ✅ | Wireframe cube + up to 500 liquid particles with collision physics |
| [`spectrum`](spectrum/) | ✅ | — | ✅ | 16-bar audio spectrum visualizer, breathing color gradient |

