# AI Usage Monitor v1.9 – clipped pixel renderer

The v1.8 on-device logs showed 44–96 ms for CPU rotation and ~38.5 ms
for panel transfer. Idle FPS is deliberately low: only refresh on changes.

## Changes

- New `include/clipped_rotation.h` fast path for nearest-neighbor rotation,
  used when motion is detected. It computes clipping per row and removes
  four per-pixel bounds checks in the hot loop. This keeps RGB565 native resolution.
- Existing bilinear rendering and QSPI transfer are unchanged.
- `AUTO_ROTATION_CLIPPED_RENDER` can be set to `0` to revert to v1.8 renderer.
- Diagnostic log reports renderer status and CPU MHz.
- IMU sampling at 100 Hz, MQTT, touch and UI unchanged.

**Not a measured speed-up** until retested on actual ESP32-S3 hardware.
The panel transfer floor of ~38.5 ms means stable 30 FPS is still not
possible with the current driver, even if rotation became free.

## Update from v1.8

Copy `src/main.cpp` and add `include/clipped_rotation.h`.
Keep your existing `include/config.h`; optionally add
`#define AUTO_ROTATION_CLIPPED_RENDER 1` (default is 1).

```powershell
pio run -e waveshare_143 -t upload
pio device monitor -b 115200
```

Compare `[PERF]` values while continuously rotating at a steady speed.
If clipped renderer performs worse, define `AUTO_ROTATION_CLIPPED_RENDER 0`.
