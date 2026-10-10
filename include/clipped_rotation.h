#pragma once
// Optional high-throughput nearest-neighbor rotation for ESP32-S3 AMOLED.
// Native 466x466 resolution, PSRAM RGB565 buffers, byte-swapped output.
// Unlike the generic renderer, source-bound checks are computed once per row;
// only the fully valid interval is sampled in the inner loop.
// Bilinear frames keep the well-tested fallback implementation.
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "fast_rotation.h"

namespace clipped_rotation {

inline int floor_div(int n, int d) {
  const int q = n / d;
  const int r = n % d;
  return q - ((r != 0) && ((r < 0) != (d < 0)));
}
inline int ceil_div(int n, int d) { return -floor_div(-n, d); }

inline bool clip_axis(int at_zero, int step, int low, int high,
                      int &xlo, int &xhi) {
  if (step == 0) return at_zero >= low && at_zero <= high;
  int lo, hi;
  if (step > 0) {
    lo = ceil_div(low - at_zero, step);
    hi = floor_div(high - at_zero, step);
  } else {
    lo = ceil_div(high - at_zero, step);
    hi = floor_div(low - at_zero, step);
  }
  if (lo > xlo) xlo = lo;
  if (hi < xhi) xhi = hi;
  return xlo <= xhi;
}

// For 466x466 circles with 16.16 coords, fixed arithmetic fits in signed int32.
// Check is applied in the public renderer for unusual future panel sizes.
inline __attribute__((optimize("O3"))) void nearest_rgb565(
    const uint16_t *src, uint16_t *dst, int w, int h, float angle_deg) {
  if (w > 1024 || h > 1024 || w <= 0 || h <= 0) {
    fast_rotation::rotate_rgb565(src,dst,w,h,angle_deg,false);
    return;
  }
  const float theta = angle_deg * (continuous_rotation::kPi / 180.f);
  const float co = cosf(theta), si = sinf(theta);
  const int32_t step_x = (int32_t)lroundf(co * 65536.f);
  const int32_t step_y = (int32_t)lroundf(-si * 65536.f);
  const float cx = (w - 1) * .5f, cy = (h - 1) * .5f;
  const float radius = fminf(w,h) * .5f;
  const float radius_sq = radius * radius;
  // Nearest rounding uses (fixed + 32768) >> 16; these are inclusive limits.
  const int32_t min_fixed = -32768;
  const int32_t max_fixed_x = (w << 16) - 32769;
  const int32_t max_fixed_y = (h << 16) - 32769;
  for (int y = 0; y < h; ++y) {
    uint16_t *out = dst + (size_t)y * w;
    const float py = y - cy;
    const float span_sq = radius_sq - py * py;
    if (span_sq <= 0) { memset(out,0,(size_t)w * 2); continue; }
    const int span = (int)sqrtf(span_sq);
    int xlo = (int)ceilf(cx - span);
    int xhi = (int)floorf(cx + span);
    if (xlo < 0) xlo = 0;
    if (xhi >= w) xhi = w - 1;
    const int32_t sx0 = (int32_t)lroundf((cx - co * cx + si * py) * 65536.f);
    const int32_t sy0 = (int32_t)lroundf((cy + si * cx + co * py) * 65536.f);
    const bool valid_x = clip_axis(sx0, step_x, min_fixed, max_fixed_x,xlo,xhi);
    const bool valid_y = valid_x && clip_axis(sy0,step_y,min_fixed,max_fixed_y,xlo,xhi);
    if (!valid_y || xlo > xhi) { memset(out,0,(size_t)w * 2); continue; }
    if (xlo) memset(out,0,(size_t)xlo * 2);
    if (xhi + 1 < w) memset(out + xhi + 1,0,(size_t)(w - xhi - 1)*2);
    int32_t sx = sx0 + step_x * xlo;
    int32_t sy = sy0 + step_y * xlo;
    for (int x = xlo; x <= xhi; ++x) {
      const unsigned u = (unsigned)((sx + 32768) >> 16);
      const unsigned v = (unsigned)((sy + 32768) >> 16);
      const uint16_t pixel = src[(size_t)v * w + u];
      out[x] = (uint16_t)((pixel << 8) | (pixel >> 8));
      sx += step_x;
      sy += step_y;
    }
  }
}

inline void render_rgb565(const uint16_t *src,uint16_t *dst,int w,int h,
                          float angle_deg,bool bilinear) {
  if (bilinear) fast_rotation::rotate_rgb565(src,dst,w,h,angle_deg,true);
  else nearest_rgb565(src,dst,w,h,angle_deg);
}
}
