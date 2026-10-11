# AI Usage Monitor v2.0 — Claude + Jadebyte

ESP32-S3 Waveshare **1.43\" / 466 × 466** — PlatformIO / LVGL 9 / WLAN / MQTT / QMI8658.

This is the v1.9 **clipped rotation** firmware with two small mascot animations and more informative gauges. It has **not yet been flash-tested on real hardware**.

<img width="200" alt="Jadebyte" src="https://github.com/user-attachments/assets/37d92b48-a90e-4fdd-bf8e-8696fa412d24" />
<img width="200" alt="Claude" src="https://github.com/user-attachments/assets/6bc2c902-ff11-4249-8d66-c087a3beb1e7" />

## Getting started

1. Copy `include/config.example.h` to `include/config.h` and enter your Wi-Fi / MQTT settings. **Do not replace your working `config.h` without saving it.**
2. Open this folder in PlatformIO in VS Code.
3. Build and upload:

   ```powershell
   pio run -e waveshare_143 -t upload
   pio device monitor -b 115200
   ```

4. `src/vendor` Waveshare drivers are fetched on first build (Internet required once). The active build is unchanged from v1.9 except the UI.
5. By default, the screen shows **DEMO** usage until retained JSON is received at `ai-monitor/state`.

## What is new

- **Claude**: friendly orange coding buddy, 3 small 120 × 120 transparent animation frames, including blink.
- **Codex**: Jadebyte-inspired panda/dragon figure, 3 frames including blink.
- **Outer arc** = **weekly remaining** (purple/magenta).
- **Inner arc** = **5-hour remaining** (orange/cyan).
- Numerical labels `5H 46% FREI` and `WOCHE 35% FREI` are not obscured.
- Optional 5h/week reset text, source freshness and connectivity status.
- Two ring backgrounds cached in PSRAM to reduce the initial slowdown when switching providers. Recomputed only when a quota changes.
- Mascot pauses during motion; gyro task / touch / panel transfer are unchanged.

## Additional settings (`include/config.h`)

```cpp
#define MASCOT_ENABLED 1              // 0 disables image and animation
#define MASCOT_IDLE_FPS 4             // 1..8 (4 recommended)
#define MASCOT_PAUSE_WHILE_TURNING 1 // best gyro responsiveness
```

If the display feels less responsive, lower `MASCOT_IDLE_FPS` to 2 or use `MASCOT_ENABLED 0`. `AUTO_ROTATION_CLIPPED_RENDER 1` should remain enabled.

## MQTT payload

Existing messages without reset fields continue to work:

```json
{
  "claude": {
    "session_remaining": 72,
    "weekly_remaining": 57,
    "session_reset": "21:30",
    "weekly_reset": "Mo 11:00"
  },
  "codex": {
    "session_remaining": 46,
    "weekly_remaining": 35,
    "session_reset": "20:00",
    "weekly_reset": "Di 09:00"
  }
}
```

All percentages are **remaining**, not used. If reset strings are not present, the display shows `--`. The existing `homeassistant/automation.yaml` still works with the same four sensors; add real reset sensor templates as desired. `homeassistant/test-payload.yaml` contains a sample manual `mqtt.publish` action.

## Sprite frames / credit

The included 120 × 120 RGBA sprites were **adapted from the AI-generated mockup approved in this conversation**, not downloaded from third-party sprite packs. They are stylized interpretations; the Codex figure is **Jadebyte-inspired**, not a byte-identical copy of `codex-pet-collection`. To replace them later with the original Jadebyte atlas (licensed CC BY 4.0), see https://github.com/shuoyang129/codex-pet-collection and credit the creator.

Frame images: `assets/claude_0.png` .. `_2.png`, `assets/jadebyte_0.png` .. `_2.png`.

To modify them, edit the PNGs (keep **120 × 120 RGBA**) and regenerate `include/mascot_frames.h`:

```powershell
python -m pip install Pillow
python tools/pack_mascots.py
```

The compiler stores frames as 32-bit ARGB in flash; only a single 57.6 KB active frame lives in PSRAM. Sprites are layered with an LVGL ARGB8888 canvas on top of the RGB565 ring canvas. No PNG decoding at runtime.

`assets/v2-layout-preview.png` is an offline layout visualization; not a screenshot from the actual ESP32.

## Notes / limitations

- The original `/5h` and `/weekly` HACS data integrations are still required for real values. No Claude/ChatGPT authentication is stored on the ESP32.
- `MASCOT_ENABLED=0` disables mascots without affecting gauges.
- A complete ESP32 build and hardware test are still needed. If you see an error, send the full first compiler message, not only the last lines.
