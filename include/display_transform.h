#pragma once
// Logical (LVGL) -> physical Waveshare panel rotation in clockwise degrees.
// Kept independent of Arduino/LVGL to allow deterministic host-side tests.
#include <stdint.h>

namespace display_transform {

constexpr bool valid_rotation(int deg) {
    return deg == 0 || deg == 90 || deg == 180 || deg == 270;
}

struct Point { int16_t x, y; };
struct Rect { int16_t x1, y1, x2, y2; };

// Touch controller reports physical glass coordinates. Map them back to the
// original LVGL coordinates (the inverse of rotating image clockwise).
inline Point touch_to_logical(int x, int y, int phys_width, int phys_height, int degrees) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= phys_width) x = phys_width - 1;
    if (y >= phys_height) y = phys_height - 1;
    switch (degrees) {
        case 90:  return {static_cast<int16_t>(y), static_cast<int16_t>(phys_width - 1 - x)};
        case 180: return {static_cast<int16_t>(phys_width - 1 - x), static_cast<int16_t>(phys_height - 1 - y)};
        case 270: return {static_cast<int16_t>(phys_height - 1 - y), static_cast<int16_t>(x)};
        default:  return {static_cast<int16_t>(x), static_cast<int16_t>(y)};
    }
}

// Translate an inclusive dirty rectangle to physical panel coordinates.
inline Rect rotate_area(Rect a, int phys_width, int phys_height, int degrees) {
    switch (degrees) {
        case 90:  return {static_cast<int16_t>(phys_width - 1 - a.y2), a.x1,
                          static_cast<int16_t>(phys_width - 1 - a.y1), a.x2};
        case 180: return {static_cast<int16_t>(phys_width - 1 - a.x2),
                          static_cast<int16_t>(phys_height - 1 - a.y2),
                          static_cast<int16_t>(phys_width - 1 - a.x1),
                          static_cast<int16_t>(phys_height - 1 - a.y1)};
        case 270: return {a.y1, static_cast<int16_t>(phys_height - 1 - a.x2),
                          a.y2, static_cast<int16_t>(phys_height - 1 - a.x1)};
        default: return a;
    }
}

constexpr uint16_t panel_rgb565(uint16_t color) {
    // Amoled::drawArea transmits MSB first but LVGL RGB565 is little-endian.
    return static_cast<uint16_t>((color << 8) | (color >> 8));
}

// Rotate a tight RGB565 tile into another (tight) buffer. Output is already
// byte-swapped for Amoled::drawArea. Source/destination must be distinct for
// 90/180/270 (in-place for 0 is allowed).
inline void rotate_tile_rgb565(const uint16_t *src, uint16_t *dst,
                               int w, int h, int degrees) {
    const int dst_width = (degrees == 90 || degrees == 270) ? h : w;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int rx = x, ry = y;
            switch (degrees) {
                case 90:  rx = h - 1 - y; ry = x; break;
                case 180: rx = w - 1 - x; ry = h - 1 - y; break;
                case 270: rx = y; ry = w - 1 - x; break;
                default: break;
            }
            dst[ry * dst_width + rx] = panel_rgb565(src[y * w + x]);
        }
    }
}
} // namespace display_transform
