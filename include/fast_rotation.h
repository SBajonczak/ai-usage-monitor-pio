#pragma once
// Faster RGB565 image rotation for ESP32-S3.
// The output uses MSB-first RGB565, matching the Waveshare Amoled::drawArea API.
// Single-precision trigonometry is evaluated per row, not per pixel.
// Pixel sampling and bilinear interpolation use fixed-point integer math.
// The whole logical LVGL picture is still rotated; nothing snaps to 90 degrees.
#include <stdint.h>
#include <math.h>
#include "continuous_rotation.h"

namespace fast_rotation {
inline __attribute__((optimize("O3"))) void rotate_rgb565(const uint16_t *src, uint16_t *dst,
                          int w, int h, float angle_deg, bool bilinear) {
  const float theta = angle_deg * (continuous_rotation::kPi / 180.f);
  const float co = cosf(theta), si = sinf(theta);
  const int32_t ixstep = static_cast<int32_t>(lroundf(co * 65536.f));
  const int32_t iystep = static_cast<int32_t>(lroundf(-si * 65536.f));
  const float cx = (w - 1) * 0.5f, cy = (h - 1) * 0.5f;
  const int32_t xmax = w << 16, ymax = h << 16;
  const float physical_radius = fminf(w, h) * 0.5f;
  const float radius_sq = physical_radius * physical_radius;

  for (int y = 0; y < h; ++y) {
    const float py = y - cy;
    int32_t sx = static_cast<int32_t>(lroundf((cx - co * cx + si * py) * 65536.f));
    int32_t sy = static_cast<int32_t>(lroundf((cy + si * cx + co * py) * 65536.f));
    uint16_t *out = dst + y * w;
    const float dy = y - cy;
    const float span_sq = radius_sq - dy * dy;
    // AMOLED is physically circular: skip the ~21% corner pixels outside glass.
    // They are written as black for deterministic contents but never visible.
    int xlo = 0, xhi = w - 1;
    if (span_sq <= 0) {
      for (int x = 0; x < w; ++x) out[x] = 0;
      continue;
    }
    const int horizontal_span = static_cast<int>(sqrtf(span_sq));
    xlo = static_cast<int>(ceilf(cx - horizontal_span));
    xhi = static_cast<int>(floorf(cx + horizontal_span));
    if (xlo < 0) xlo = 0;
    if (xhi >= w) xhi = w - 1;
    for (int x = 0; x < xlo; ++x) out[x] = 0;
    for (int x = xhi + 1; x < w; ++x) out[x] = 0;
    sx += ixstep * xlo;
    sy += iystep * xlo;
    for (int x = xlo; x <= xhi; ++x) {
      uint16_t result = 0;
      if (bilinear) {
        const int u = sx >> 16, v = sy >> 16;
        if (u >= 0 && v >= 0 && u < w - 1 && v < h - 1) {
          const unsigned fx = (static_cast<uint32_t>(sx) & 65535U) >> 8;
          const unsigned fy = (static_cast<uint32_t>(sy) & 65535U) >> 8;
          const int i = v * w + u;
          result = continuous_rotation::lerp565(src[i], src[i+1],
                                                  src[i+w], src[i+w+1], fx, fy);
        } else if (sx >= 0 && sy >= 0 && sx < xmax && sy < ymax) {
          int u0 = (sx + 32768) >> 16;
          int v0 = (sy + 32768) >> 16;
          if (u0 >= w) u0 = w - 1;
          if (v0 >= h) v0 = h - 1;
          result = src[v0 * w + u0];
        }
      } else {
        const int u = (sx + 32768) >> 16;
        const int v = (sy + 32768) >> 16;
        if (u >= 0 && u < w && v >= 0 && v < h) result = src[v*w + u];
      }
      out[x] = continuous_rotation::swapped565(result);
      sx += ixstep;
      sy += iystep;
    }
  }
}
}
