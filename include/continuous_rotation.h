#pragma once
// No Arduino includes: testable on desktop. Angles clockwise-positive in
// display coordinates, relative to configured startup orientation.
#include <stdint.h>
#include <math.h>

namespace continuous_rotation {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kRadToDeg = 180.0f / kPi;
inline float wrap360(float a) {
  a = fmodf(a, 360.0f);
  return a < 0.0f ? a + 360.0f : a;
}
inline float wrap180(float a) {
  a = fmodf(a + 180.0f, 360.0f);
  if (a < 0.0f) a += 360.0f;
  return a - 180.0f;
}
inline float shortest_difference(float to, float from) {
  return wrap180(to - from);
}
struct Point { int16_t x, y; };

// Inverse of clockwise rendered image rotation: physical touchscreen -> LVGL.
inline Point touch_to_logical(float x, float y, int w, int h, float angleDegrees) {
  const float angle = angleDegrees / kRadToDeg;
  const float c = cosf(angle), s = sinf(angle);
  const float cx = (w - 1) * 0.5f, cy = (h - 1) * 0.5f;
  const float dx = x - cx, dy = y - cy;
  int sx = int(lroundf(cx + c * dx + s * dy));
  int sy = int(lroundf(cy - s * dx + c * dy));
  if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
  if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
  return {static_cast<int16_t>(sx),static_cast<int16_t>(sy)};
}
constexpr uint16_t swapped565(uint16_t c) {
  return (uint16_t)((c << 8) | (c >> 8));
}
inline uint16_t lerp565(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
                        unsigned fx, unsigned fy) {
  const unsigned wx = 256U - fx, wy = 256U - fy;
  const unsigned w00 = wx * wy, w10 = fx * wy;
  const unsigned w01 = wx * fy, w11 = fx * fy;
  const uint32_t r = (((a >> 11) & 31)*w00 + ((b >> 11) & 31)*w10 +
                      ((c >> 11) & 31)*w01 + ((d >> 11) & 31)*w11 + 32768) >> 16;
  const uint32_t g = (((a >> 5) & 63)*w00 + ((b >> 5) & 63)*w10 +
                      ((c >> 5) & 63)*w01 + ((d >> 5) & 63)*w11 + 32768) >> 16;
  const uint32_t blue = ((a & 31)*w00 + (b & 31)*w10 +
                         (c & 31)*w01 + (d & 31)*w11 + 32768) >> 16;
  return (uint16_t)(((r > 31 ? 31 : r) << 11) |
                    ((g > 63 ? 63 : g) << 5) | (blue > 31 ? 31 : blue));
}

// RGB565 in host-endian (LVGL) in; MSB-first (Waveshare panel) out.
// Full buffer is required: with non-90-degree rotation, a logical dirty rectangle
// doesn't map to one rectangular physical panel region.
inline void render_rotated_rgb565(const uint16_t *logical, uint16_t *physical,
                                   int w, int h, float angleDegrees, bool bilinear) {
  const float a = angleDegrees / kRadToDeg, c = cosf(a), s = sinf(a);
  const float cx = (w-1) * .5f, cy = (h-1) * .5f;
  for (int y=0; y<h; ++y) {
    const float py = y - cy;
    float sx = cx + c*(-cx) + s*py;
    float sy = cy - s*(-cx) + c*py;
    for (int x=0; x<w; ++x) {
      uint16_t pixel = 0;
      if (bilinear) {
        const int ix = (int)floorf(sx), iy = (int)floorf(sy);
        if (ix >= 0 && iy >= 0 && ix < w - 1 && iy < h - 1) {
          const unsigned fx = (unsigned)((sx - ix) * 256.f);
          const unsigned fy = (unsigned)((sy - iy) * 256.f);
          const int i = iy*w + ix;
          pixel = lerp565(logical[i], logical[i+1],
                          logical[i+w], logical[i+w+1], fx, fy);
        } else if (sx >= 0 && sy >= 0 && sx < w && sy < h) {
          const int nx = (int)lroundf(sx), ny = (int)lroundf(sy);
          pixel = logical[(ny >= h ? h - 1 : ny) * w + (nx >= w ? w - 1 : nx)];
        }
      } else {
        const int ix = (int)lroundf(sx), iy = (int)lroundf(sy);
        if (ix>=0 && iy>=0 && ix<w && iy<h) pixel = logical[iy*w+ix];
      }
      physical[y*w+x] = swapped565(pixel);
      sx += c;
      sy -= s;
    }
  }
}

class ComplementaryRoll {
  bool initialized_ = false;
  float reference_ = 0.0f; // gravity direction at startup
  float degrees_ = 0.0f;
public:
  float angleDegrees() const { return degrees_; }
  void reset() { initialized_ = false; degrees_ = 0; }
  void update(float ax, float ay, float az, float signedGyroZRadPerSecond,
              float dt, float accelGain, float maxAccelError) {
    if (!(dt > 0.0f && dt <= 0.25f)) return;
    const float norm = sqrtf(ax*ax+ay*ay+az*az);
    const float xy = sqrtf(ax*ax+ay*ay);
    const bool gravityReliable = (fabsf(norm - 9.807f) <= maxAccelError) && xy > 3.2f;
    // Integrate gyro immediately. Gravity is only used to correct drift.
    degrees_ = wrap180(degrees_ + signedGyroZRadPerSecond * dt * kRadToDeg);
    if (gravityReliable) {
      const float measured = atan2f(ay,ax) * kRadToDeg;
      if (!initialized_) {
        reference_ = measured;
        initialized_ = true;
      }
      const float absoluteRelative = wrap180(measured - reference_);
      const float correction = shortest_difference(absoluteRelative,degrees_);
      degrees_ = wrap180(degrees_ + accelGain * correction);
    }
  }
};
}
