/*
 * AI Usage Monitor - Premium UI 1.9 (clipped rotation) - PlatformIO C++ firmware
 * Target: Waveshare ESP32-S3-Touch-AMOLED-1.43, 466x466
 * BSP implementation loaded from:
 * https://github.com/thelastoutpostworkshop/waveshare_esp32s3_1.43_amoled_lvgl9
 * Library sources are fetched on the first PlatformIO build.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <lvgl.h>
#include "vendor/amoled.h"
#include "vendor/FT3168.h"
#include "vendor/qmi8658c.h"
#include "config.h"
#include "display_transform.h"
#include "continuous_rotation.h"
#include "fast_rotation.h"
#include "clipped_rotation.h"
#include "motion_prediction.h"

// Clockwise picture rotation on the physical panel. Keep as 90 for USB below.
// Override in include/config.h with 0, 90, 180, or 270.
#ifndef DISPLAY_ROTATION_DEG
#define DISPLAY_ROTATION_DEG 90
#endif
static_assert(display_transform::valid_rotation(DISPLAY_ROTATION_DEG),
              "DISPLAY_ROTATION_DEG must be 0, 90, 180, or 270");

// ---------- LVGL display / continuous rotation ----------
static Amoled amoled;
static lv_display_t *disp = nullptr;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;
static constexpr size_t BUF_BYTES = DISPLAY_WIDTH * 40 * sizeof(lv_color_t);
static constexpr size_t FRAME_PIXELS = DISPLAY_WIDTH * DISPLAY_HEIGHT;
// PSRAM: LVGL-composited logical picture + panel-ready rotated RGB565 picture.
static uint16_t *logicalFrame = nullptr;
static uint16_t *panelFrame = nullptr;
static bool pictureDirty = true;
static bool autoRotationAvailable = false;
static float displayAngleDeg = float(DISPLAY_ROTATION_DEG);
static float renderedAngleDeg = -9999.0f;
static bool renderedBilinear = false;
// Dedicated sensor task runs independently from QSPI panel transfer.
static SemaphoreHandle_t i2cMutex = nullptr;
static portMUX_TYPE sensorMux = portMUX_INITIALIZER_UNLOCKED;
static float sensorRoll = 0.0f;
static float sensorSpeedDps = 0.0f;
static uint32_t sensorTimeMs = 0;
static float observedSpeedDps = 0.0f;
static uint32_t lastMotionMs = 0;
static uint32_t frameLastMs = 0;
static uint32_t imuLogLastMs = 0;
static uint32_t perfLogLastMs = 0;
static uint32_t perfFrames = 0;
static uint64_t perfRenderUs = 0;
static uint64_t perfTransferUs = 0;

// 1: scanline-clipped motion renderer; 0: previous v1.8 renderer
#ifndef AUTO_ROTATION_CLIPPED_RENDER
#define AUTO_ROTATION_CLIPPED_RENDER 1
#endif
#ifndef AUTO_ROTATION_ENABLED
#define AUTO_ROTATION_ENABLED 1
#endif
#ifndef AUTO_ROTATION_FPS
#define AUTO_ROTATION_FPS 30
#endif
#ifndef AUTO_ROTATION_ADAPTIVE_QUALITY
#define AUTO_ROTATION_ADAPTIVE_QUALITY 1
#endif
#ifndef AUTO_ROTATION_ANGLE_EPSILON
#define AUTO_ROTATION_ANGLE_EPSILON 0.20f
#endif
#ifndef AUTO_ROTATION_LOG_PERF
#define AUTO_ROTATION_LOG_PERF 1
#endif
#ifndef AUTO_ROTATION_ACCEL_SIGN
#define AUTO_ROTATION_ACCEL_SIGN 1
#endif
#ifndef AUTO_ROTATION_GYRO_SIGN
#define AUTO_ROTATION_GYRO_SIGN -1
#endif
#ifndef AUTO_ROTATION_BILINEAR
#define AUTO_ROTATION_BILINEAR 1
#endif
#ifndef AUTO_ROTATION_ACCEL_GAIN
#define AUTO_ROTATION_ACCEL_GAIN 0.035f
#endif
#ifndef AUTO_ROTATION_MAX_ACCEL_ERROR
#define AUTO_ROTATION_MAX_ACCEL_ERROR 2.5f
#endif
static_assert(AUTO_ROTATION_FPS >= 1 && AUTO_ROTATION_FPS <= 60,
              "AUTO_ROTATION_FPS must be 1..60");
static_assert(AUTO_ROTATION_ACCEL_SIGN == 1 || AUTO_ROTATION_ACCEL_SIGN == -1,
              "AUTO_ROTATION_ACCEL_SIGN must be +1 or -1");
static_assert(AUTO_ROTATION_GYRO_SIGN == 1 || AUTO_ROTATION_GYRO_SIGN == -1,
              "AUTO_ROTATION_GYRO_SIGN must be +1 or -1");

#ifndef AUTO_ROTATION_SENSOR_HZ
#define AUTO_ROTATION_SENSOR_HZ 100
#endif
#ifndef AUTO_ROTATION_PREDICT_MS
#define AUTO_ROTATION_PREDICT_MS 15
#endif
#ifndef AUTO_ROTATION_STILL_MS
#define AUTO_ROTATION_STILL_MS 120
#endif
#ifndef AUTO_ROTATION_MOVEMENT_DPS
#define AUTO_ROTATION_MOVEMENT_DPS 4.0f
#endif
static_assert(AUTO_ROTATION_SENSOR_HZ >= 20 && AUTO_ROTATION_SENSOR_HZ <= 200,
              "AUTO_ROTATION_SENSOR_HZ must be between 20 and 200");
static_assert(AUTO_ROTATION_PREDICT_MS >= 0 && AUTO_ROTATION_PREDICT_MS <= 50,
              "AUTO_ROTATION_PREDICT_MS must be between 0 and 50");

static uint32_t tick_cb() { return millis(); }

static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
  // LVGL partial RGB565 tiles are combined into one logical frame in PSRAM.
  // The panel is updated later from a full rotated snapshot. Do NOT swap bytes here.
  const uint16_t *src = reinterpret_cast<const uint16_t *>(pixels);
  const int w = lv_area_get_width(area), h = lv_area_get_height(area);
  for (int row = 0; row < h; ++row) {
    memcpy(logicalFrame + (area->y1 + row) * DISPLAY_WIDTH + area->x1,
           src + row * w, w * sizeof(uint16_t));
  }
  pictureDirty = true;
  lv_display_flush_ready(display);
}

// Panel controller needs x/y even-aligned rectangles.
static void rounder_cb(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_INVALIDATE_AREA) return;
  auto *a = static_cast<lv_area_t *>(lv_event_get_param(event));
  if (!a) return;
  a->x1 &= ~1; a->x2 |= 1;
  a->y1 &= ~1; a->y2 |= 1;
}

static void touch_cb(lv_indev_t *, lv_indev_data_t *data) {
  uint16_t x = 0, y = 0;
  // Touch and QMI8658 share I2C pins; never read concurrently.
  // If the IMU holds the bus, keep last touch state instead of
  // generating an accidental release/click.
  if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(4)) != pdTRUE) {
    return;
  }
  const bool pressed = getTouch(&x, &y);
  if (i2cMutex) xSemaphoreGive(i2cMutex);
  if (!pressed) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  const auto pt = continuous_rotation::touch_to_logical(
      x, y, DISPLAY_WIDTH, DISPLAY_HEIGHT, displayAngleDeg);
  data->point.x = pt.x;
  data->point.y = pt.y;
  data->state = LV_INDEV_STATE_PRESSED;
}

// Read IMU continuously on the other ESP32-S3 core so SPI transfers
// cannot stall motion samples. Only very small state snapshots use spinlock.
static void motionTask(void *) {
  continuous_rotation::ComplementaryRoll filter;
  TickType_t lastWake = xTaskGetTickCount();
  uint32_t lastSample = 0;
  while (true) {
    float acc[3] = {}, gyro[3] = {};
    const uint32_t now = millis();
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(8)) == pdTRUE) {
      qmi8658_read_xyz(acc, gyro);
      xSemaphoreGive(i2cMutex);
      const float dt = lastSample ? fminf((now - lastSample) * .001f, .10f) : .01f;
      lastSample = now;
      if (isfinite(acc[0]) && isfinite(acc[1]) && isfinite(acc[2]) &&
          isfinite(gyro[2])) {
        const float signedGz = gyro[2] * AUTO_ROTATION_GYRO_SIGN;
        filter.update(acc[0] * AUTO_ROTATION_ACCEL_SIGN, acc[1], acc[2],
                      signedGz, dt, AUTO_ROTATION_ACCEL_GAIN,
                      AUTO_ROTATION_MAX_ACCEL_ERROR);
        portENTER_CRITICAL(&sensorMux);
        sensorRoll = filter.angleDegrees();
        sensorSpeedDps = signedGz * continuous_rotation::kRadToDeg;
        sensorTimeMs = now;
        portEXIT_CRITICAL(&sensorMux);
      }
    }
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000 / AUTO_ROTATION_SENSOR_HZ));
  }
}

static void sampleMotion() {
  if (!autoRotationAvailable) return;
  float roll = 0.0f, speed = 0.0f;
  uint32_t timeMs = 0;
  portENTER_CRITICAL(&sensorMux);
  roll = sensorRoll;
  speed = sensorSpeedDps;
  timeMs = sensorTimeMs;
  portEXIT_CRITICAL(&sensorMux);
  const uint32_t now = millis();
  if (timeMs == 0 || now - timeMs > 300) return;
  observedSpeedDps = speed;
  // Predict the short transfer latency only, never extrapolate stale readings.
  // Excessive prediction makes the UI swing past the user's hand.
  const uint32_t sampleAgeMs = now - timeMs;
  displayAngleDeg = motion_prediction::predict(
      DISPLAY_ROTATION_DEG, roll, speed,
      sampleAgeMs, AUTO_ROTATION_PREDICT_MS);
  if (now - imuLogLastMs > 5000) {
    imuLogLastMs = now;
    Serial.printf("[IMU] roll %.1f deg | speed %.1f deg/s | age %lu ms\n",
                  roll, speed, (unsigned long)sampleAgeMs);
  }
}

static void updatePhysicalPanel() {
  const uint32_t now = millis();
  if (now - frameLastMs < (1000U / AUTO_ROTATION_FPS)) return;
  const float angleDelta = fabsf(continuous_rotation::shortest_difference(
      displayAngleDeg, renderedAngleDeg));
  const bool changedAngle = angleDelta > AUTO_ROTATION_ANGLE_EPSILON;
  // During motion, nearest-neighbor is much cheaper than four PSRAM reads
  // plus bilinear blending per pixel. When motion stops, automatically draw
  // one final antialiased frame. All sampling remains at native 466x466.
  // Quality hysteresis: don't swap nearest/bilinear on alternating
  // frames (visible as edge shimmer). Remain fast during motion; polish
  // once after the device has settled for AUTO_ROTATION_STILL_MS.
  const bool moving = fabsf(observedSpeedDps) > AUTO_ROTATION_MOVEMENT_DPS ||
                      angleDelta > 0.45f;
  if (moving) lastMotionMs = now;
  const bool interpolate = (AUTO_ROTATION_BILINEAR != 0) &&
      !(AUTO_ROTATION_ADAPTIVE_QUALITY &&
        (uint32_t)(now - lastMotionMs) < AUTO_ROTATION_STILL_MS);
  const bool qualityChanged = renderedBilinear != interpolate;
  if (!pictureDirty && !changedAngle && !qualityChanged) return;
  frameLastMs = now;
  // Snapshot angle before drawing; IMU polling during transfer must not
  // change which angle we claim has already been displayed.
  const float frameAngleDeg = displayAngleDeg;
  // full screen rotation, including LVGL labels and ring backdrop
  const uint32_t rotateStartUs = micros();
#if AUTO_ROTATION_CLIPPED_RENDER
  clipped_rotation::render_rgb565(
      logicalFrame, panelFrame, DISPLAY_WIDTH, DISPLAY_HEIGHT,
      frameAngleDeg, interpolate);
#else
  fast_rotation::rotate_rgb565(
      logicalFrame, panelFrame, DISPLAY_WIDTH, DISPLAY_HEIGHT,
      frameAngleDeg, interpolate);
#endif
  const uint32_t rotateDurationUs = micros() - rotateStartUs;
  const uint32_t transferStartUs = micros();
  // Send in even row strips: avoids a single huge DMA transfer and
  // works with Waveshare's QSPI partial-area transfer API.
  constexpr int BLOCK_ROWS = 16;
  for (int y = 0; y < DISPLAY_HEIGHT; y += BLOCK_ROWS) {
    const int y2 = (y + BLOCK_ROWS <= DISPLAY_HEIGHT)
      ? y + BLOCK_ROWS - 1 : DISPLAY_HEIGHT - 1;
    amoled.drawArea(0, y, DISPLAY_WIDTH - 1, y2,
                    panelFrame + y * DISPLAY_WIDTH);
  }
  const uint32_t transferDurationUs = micros() - transferStartUs;
  ++perfFrames;
  perfRenderUs += rotateDurationUs;
  perfTransferUs += transferDurationUs;
#if AUTO_ROTATION_LOG_PERF
  if (now - perfLogLastMs > 5000) {
    const uint32_t elapsed = now - perfLogLastMs;
    Serial.printf("[PERF] %.1f fps | rotate %.1f ms | panel %.1f ms | heap %u | psram %u\n",
                  perfFrames * 1000.f / elapsed,
                  perfRenderUs / (perfFrames * 1000.f),
                  perfTransferUs / (perfFrames * 1000.f),
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    perfLogLastMs = now;
    perfFrames = 0;
    perfRenderUs = 0;
    perfTransferUs = 0;
  }
#endif
  renderedAngleDeg = frameAngleDeg;
  renderedBilinear = interpolate;
  pictureDirty = false;
}

// ---------- Usage state ----------
struct Usage {
  int session = -1; // percent REMAINING, -1 means unavailable
  int weekly = -1;
  bool live = false;
  uint32_t receivedMs = 0;
};
static Usage claude = {72, 57, false, 0}; // explicitly DEMO until MQTT arrives
static Usage codex  = {46, 35, false, 0};
static bool showCodex = true; // Start with the Codex look from the reference photo
// High-quality ring background, drawn at full native 466x466 resolution.
// Each ring edge is covered by a smooth, fractional-pixel alpha gradient.
static lv_obj_t *ringCanvas = nullptr;
static uint16_t *ringPixels = nullptr; // ~424 KiB in PSRAM
static int drawnProvider = -1, drawnSession = -2, drawnWeek = -2;
static lv_obj_t *titleLabel = nullptr;
static lv_obj_t *numberLabel = nullptr;
static lv_obj_t *weekValueLabel = nullptr;
static lv_obj_t *weekCard = nullptr;
static lv_obj_t *sourceLabel = nullptr;
static lv_obj_t *networkLabel = nullptr;
static lv_obj_t *mqttLabel = nullptr;
static lv_obj_t *statusDot = nullptr;
static constexpr uint32_t STALE_MS = 20UL * 60UL * 1000UL;

static WiFiClient net;
static PubSubClient mqtt(net);
static uint32_t lastWifiDiagnostic = 0;
static uint32_t lastMqttAttempt = 0;
static uint32_t lastUiRefresh = 0;

// WLAN-Verbindungsversuche werden ausschließlich vom Arduino-Core behandelt.
// Die Eventfunktion läuft in einem anderen Task: keine LVGL-Aufrufe hier!
static const char *wifiReasonText(uint8_t reason) {
  switch (reason) {
    case 200: return "BEACON_TIMEOUT (Signal / Access Point)";
    case 201: return "NO_AP_FOUND (SSID / 2.4 GHz / Signal)";
    case 202: return "AUTH_FAIL (Passwort / WPA-Konfiguration)";
    case 203: return "ASSOC_FAIL (AP lehnt Anmeldung ab)";
    case 204: return "HANDSHAKE_TIMEOUT (WPA / Signal)";
    default:  return "siehe ESP-IDF WiFi reason codes";
  }
}

static void wifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WLAN] Mit Access Point verbunden; warte auf IP-Adresse...");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WLAN] Verbunden. IP: %s\n", WiFi.localIP().toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      const uint8_t reason = info.wifi_sta_disconnected.reason;
      Serial.printf("[WLAN] Verbindung getrennt. Grund %u: %s\n",
                    reason, wifiReasonText(reason));
      break;
    }
    default:
      break;
  }
}

static bool validPercent(int v) { return v >= 0 && v <= 100; }
static bool isStale(const Usage &u) {
  return u.live && (uint32_t)(millis() - u.receivedMs) > STALE_MS;
}
// ----- Smooth custom RGB565 ring renderer -----
// Standard lv_arc was visibly stair-stepped on this small round AMOLED.
// Here an analytic coverage function anti-aliases each ring edge and its
// rounded ends before quantization to RGB565. No scaling or bitmaps.
struct Rgb { float r, g, b; };
static constexpr float kRingTwoPi = 6.28318530718f;
static float saturate(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static void blend(Rgb &dst, const Rgb &src, float alpha) {
  alpha = saturate(alpha);
  dst.r += (src.r - dst.r) * alpha;
  dst.g += (src.g - dst.g) * alpha;
  dst.b += (src.b - dst.b) * alpha;
}
static uint16_t pack565(const Rgb &rgb) {
  const uint16_t r = static_cast<uint8_t>(rgb.r) >> 3;
  const uint16_t g = static_cast<uint8_t>(rgb.g) >> 2;
  const uint16_t b = static_cast<uint8_t>(rgb.b) >> 3;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}
static float circleCoverage(float dx, float dy, float halfWidth) {
  return saturate(halfWidth + 0.5f - sqrtf(dx * dx + dy * dy));
}

// The reference design uses two independent sweeps rather than two full
// circles that both begin at 12 o'clock. The 5h arc is teal and grows from
// the lower-left towards the top. The weekly arc is magenta on the right.
static void paintRing(Rgb &pixel, float dx, float dy, float radius,
                      float width, int remaining, const Rgb &track,
                      const Rgb &progress, float startDeg) {
  const float d = sqrtf(dx * dx + dy * dy);
  const float offset = fabsf(d - radius);
  if (offset > width * 0.5f + 7.0f) return;
  const float edge = saturate(width * 0.5f + 0.5f - offset);
  blend(pixel, track, edge);
  if (!validPercent(remaining) || remaining <= 0) return;

  const float start = startDeg * (kRingTwoPi / 360.0f);
  const float sweep = kRingTwoPi * (remaining / 100.0f);
  float angle = atan2f(dx, -dy); // 0 at top, clockwise positive
  if (angle < 0) angle += kRingTwoPi;
  float relative = angle - start;
  if (relative < 0) relative += kRingTwoPi;

  float fill = (remaining >= 100 || relative <= sweep) ? edge : 0.0f;
  // Rounded antialiased caps; for 100% the full track is simply colored.
  if (remaining < 100) {
    const float sx = radius * sinf(start);
    const float sy = -radius * cosf(start);
    const float end = start + sweep;
    const float ex = radius * sinf(end);
    const float ey = -radius * cosf(end);
    fill = fmaxf(fill, circleCoverage(dx - sx, dy - sy, width * 0.5f));
    fill = fmaxf(fill, circleCoverage(dx - ex, dy - ey, width * 0.5f));
  }
  // Gentle glow only near the actual progress arc, never over all of the
  // black background. This is deliberately subtle on an AMOLED panel.
  const float halo = saturate((width * 0.5f + 5.0f - offset) / 5.0f) * 0.09f;
  if (fill > 0.0f) blend(pixel, progress, halo);
  blend(pixel, progress, fill);
}

static void drawSmoothRings(int provider, int session, int weekly) {
  if (!ringPixels || !ringCanvas) return;
  const bool codexTheme = provider == 1;
  const Rgb background = {7, 11, 19};
  const Rgb track = {49, 66, 87};
  const Rgb sessionColor = codexTheme ? Rgb{27, 221, 237} : Rgb{246, 169, 111};
  const Rgb weeklyColor = codexTheme ? Rgb{217, 64, 232} : Rgb{189, 128, 248};
  constexpr float OUTER_R = 214.0f, INNER_R = 186.0f;
  constexpr float OUTER_W = 15.0f, INNER_W = 14.0f;
  constexpr float C = DISPLAY_WIDTH / 2.0f;
  for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
    const float dy = (y + 0.5f) - C;
    for (int x = 0; x < DISPLAY_WIDTH; ++x) {
      const float dx = (x + 0.5f) - C;
      const float d = sqrtf(dx * dx + dy * dy);
      const float innerGlow = saturate(1.0f - d / 245.0f);
      Rgb rgb = background;
      // Very soft charcoal/navy center instead of the old flat red-looking
      // background. Correct RGB565 byte order is applied in flush_cb().
      rgb.r += 7.0f * innerGlow;
      rgb.g += 10.0f * innerGlow;
      rgb.b += 16.0f * innerGlow;
      paintRing(rgb, dx, dy, OUTER_R, OUTER_W, weekly,
                track, weeklyColor, 5.0f);
      paintRing(rgb, dx, dy, INNER_R, INNER_W, session,
                track, sessionColor, 224.0f);
      ringPixels[y * DISPLAY_WIDTH + x] = pack565(rgb);
    }
  }
  lv_obj_invalidate(ringCanvas);
}

static lv_obj_t *makeLabel(lv_obj_t *parent, const char *label,
                           int x, int y, lv_color_t color,
                           const lv_font_t *font) {
  lv_obj_t *obj = lv_label_create(parent);
  lv_label_set_text(obj, label);
  lv_obj_set_style_text_color(obj, color, 0);
  lv_obj_set_style_text_font(obj, font, 0);
  lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(obj, LV_ALIGN_CENTER, x, y);
  return obj;
}

static void updateUi() {
  const Usage &u = showCodex ? codex : claude;
  const int provider = showCodex ? 1 : 0;
  const lv_color_t accent = lv_color_hex(showCodex ? 0x26DCEE : 0xF6A96F);
  const bool stale = isStale(u);
  lv_label_set_text(titleLabel, showCodex ? "CODEX" : "CLAUDE");
  lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xF4F7FF), 0);
  const int s = validPercent(u.session) ? u.session : -1;
  const int w = validPercent(u.weekly) ? u.weekly : -1;
  if (provider != drawnProvider || s != drawnSession || w != drawnWeek) {
    drawSmoothRings(provider, s, w);
    drawnProvider = provider;
    drawnSession = s;
    drawnWeek = w;
  }
  if (s >= 0) lv_label_set_text_fmt(numberLabel, "%d%%", s);
  else lv_label_set_text(numberLabel, "--");
  if (w >= 0) lv_label_set_text_fmt(weekValueLabel, "%d%% FREI", w);
  else lv_label_set_text(weekValueLabel, "--");
  lv_obj_set_style_text_color(weekValueLabel, accent, 0);

  if (!u.live) lv_label_set_text(sourceLabel, "DEMO  /  TIPPEN ZUM WECHSELN");
  else if (stale) lv_label_set_text(sourceLabel, "VERALTET  /  TIPPEN ZUM WECHSELN");
  else lv_label_set_text(sourceLabel, "TIPPEN ZUM WECHSELN");

  const bool wifiOk = WiFi.status() == WL_CONNECTED;
  const bool mqttOk = mqtt.connected();
  lv_label_set_text(networkLabel, wifiOk ? "ONLINE" : "OFFLINE");
  lv_label_set_text(mqttLabel, mqttOk ? "MQTT OK" : "MQTT --");
  lv_obj_set_style_bg_color(statusDot, lv_color_hex(mqttOk ? 0x3FDE8B : 0xE38F60), 0);
}

static void onScreenEvent(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    showCodex = !showCodex;
    updateUi();
  }
}
static void createUi() {
  lv_obj_t *root = lv_screen_active();
  lv_obj_set_style_bg_color(root, lv_color_hex(0x080D15), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  ringCanvas = lv_canvas_create(root);
  lv_canvas_set_buffer(ringCanvas, ringPixels, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                       LV_COLOR_FORMAT_RGB565);
  lv_obj_center(ringCanvas);
  lv_obj_remove_flag(ringCanvas, LV_OBJ_FLAG_CLICKABLE);

  titleLabel = makeLabel(root, "CLAUDE", 0, -104, lv_color_hex(0xF4F7FF), &lv_font_montserrat_24);
  lv_obj_set_style_text_letter_space(titleLabel, 4, 0);
  numberLabel = makeLabel(root, "72%", 0, -43, lv_color_hex(0xFCFDFF), &lv_font_montserrat_48);
  // Narrow subtitle provides more breathing room around the central value.
  lv_obj_t *subtitle = makeLabel(root, "5-STUNDEN-LIMIT", 0, 12,
                                 lv_color_hex(0x98A7BE), &lv_font_montserrat_18);
  lv_obj_set_style_text_letter_space(subtitle, 2, 0);

  weekCard = lv_obj_create(root);
  lv_obj_set_size(weekCard, 286, 50);
  lv_obj_align(weekCard, LV_ALIGN_CENTER, 0, 76);
  lv_obj_set_style_radius(weekCard, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(weekCard, lv_color_hex(0x151E2D), 0);
  lv_obj_set_style_bg_opa(weekCard, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(weekCard, lv_color_hex(0x33435D), 0);
  lv_obj_set_style_border_width(weekCard, 1, 0);
  lv_obj_set_style_pad_all(weekCard, 0, 0);
  lv_obj_remove_flag(weekCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(weekCard, LV_OBJ_FLAG_CLICKABLE);
  makeLabel(weekCard, "WOCHE", -77, 0, lv_color_hex(0xBAC7D8), &lv_font_montserrat_18);
  lv_obj_t *separator = lv_obj_create(weekCard);
  lv_obj_set_size(separator, 1, 24);
  lv_obj_set_style_bg_color(separator, lv_color_hex(0x4B5B74), 0);
  lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(separator, 0, 0);
  lv_obj_align(separator, LV_ALIGN_CENTER, -14, 0);
  weekValueLabel = makeLabel(weekCard, "57% FREI", 64, 0,
                             lv_color_hex(0x26DCEE), &lv_font_montserrat_18);

  sourceLabel = makeLabel(root, "DEMO  /  TIPPEN ZUM WECHSELN", 0, 138,
                          lv_color_hex(0xA3B3CA), &lv_font_montserrat_14);
  lv_obj_set_style_text_letter_space(sourceLabel, 1, 0);
  networkLabel = makeLabel(root, "OFFLINE", -64, 168,
                           lv_color_hex(0x899AAF), &lv_font_montserrat_14);
  statusDot = lv_obj_create(root);
  lv_obj_set_size(statusDot, 7, 7);
  lv_obj_set_style_radius(statusDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(statusDot, lv_color_hex(0xE38F60), 0);
  lv_obj_set_style_bg_opa(statusDot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(statusDot, 0, 0);
  lv_obj_set_style_pad_all(statusDot, 0, 0);
  lv_obj_align(statusDot, LV_ALIGN_CENTER, 0, 168);
  mqttLabel = makeLabel(root, "MQTT --", 69, 168,
                        lv_color_hex(0x899AAF), &lv_font_montserrat_14);

  // Full-screen transparent touch zone. Exactly one tap changes provider.
  lv_obj_t *tapLayer = lv_obj_create(root);
  lv_obj_set_size(tapLayer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_center(tapLayer);
  lv_obj_set_style_bg_opa(tapLayer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tapLayer, 0, 0);
  lv_obj_set_style_outline_width(tapLayer, 0, 0);
  lv_obj_set_style_shadow_width(tapLayer, 0, 0);
  lv_obj_set_style_pad_all(tapLayer, 0, 0);
  lv_obj_remove_flag(tapLayer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(tapLayer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(tapLayer, onScreenEvent, LV_EVENT_CLICKED, nullptr);
  updateUi();
}

// ---------- MQTT: retained JSON message at ai-monitor/state ----------
static void applyUsage(JsonVariantConst source, Usage &target) {
  if (source.isNull()) return;
  const int s = source["session_remaining"].is<int>()
    ? source["session_remaining"].as<int>() : -1;
  const int w = source["weekly_remaining"].is<int>()
    ? source["weekly_remaining"].as<int>() : -1;
  // Do not accidentally turn an HA unavailable sensor into 0% remaining.
  if (!validPercent(s) && !validPercent(w)) return;
  target.session = validPercent(s) ? s : -1;
  target.weekly = validPercent(w) ? w : -1;
  target.live = true;
  target.receivedMs = millis();
}
static void mqttCallback(char *topic, byte *payload, unsigned int size) {
  if (strcmp(topic, MQTT_TOPIC) != 0) return;
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, size);
  if (error) { Serial.printf("MQTT JSON error: %s\n", error.c_str()); return; }
  applyUsage(doc["claude"].as<JsonVariantConst>(), claude);
  applyUsage(doc["codex"].as<JsonVariantConst>(), codex);
  updateUi();
}
static void maintainNetwork() {
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    // WiFi.setAutoReconnect(true) übernimmt den Neuaufbau nach DISCONNECTED.
    // KEIN zusätzliches WiFi.reconnect(): das erzeugt "sta is connecting".
    if (now - lastWifiDiagnostic >= 15000) {
      lastWifiDiagnostic = now;
      Serial.printf("[WLAN] Noch offline, Status=%d. Warte auf Auto-Reconnect.\n",
                    static_cast<int>(WiFi.status()));
    }
    return;
  }
  if (!mqtt.connected()) {
    if (now - lastMqttAttempt >= 5000) {
      lastMqttAttempt = now;
      String clientId = "ai-monitor-" + String((uint32_t)ESP.getEfuseMac(), HEX);
      bool connected = false;
      if (strlen(MQTT_USER) > 0) connected = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
      else connected = mqtt.connect(clientId.c_str());
      if (connected) {
        mqtt.subscribe(MQTT_TOPIC, 1);
        Serial.println("MQTT connected, subscribed");
      } else {
        Serial.printf("MQTT connect failed: state %d\n", mqtt.state());
      }
    }
  } else {
    mqtt.loop();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Touch_Init();
  if (!amoled.begin()) {
    Serial.println("AMOLED initialization failed");
    while (true) delay(1000);
  }

  lv_init();
  lv_tick_set_cb(tick_cb);
  buf1 = (lv_color_t *)heap_caps_malloc(BUF_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  buf2 = (lv_color_t *)heap_caps_malloc(BUF_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf1 || !buf2) {
    Serial.println("PSRAM allocation failed. Enable OPI PSRAM in board settings.");
    while (true) delay(1000);
  }
  logicalFrame = (uint16_t *)heap_caps_calloc(
      FRAME_PIXELS, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  panelFrame = (uint16_t *)heap_caps_malloc(
      FRAME_PIXELS * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!logicalFrame || !panelFrame) {
    Serial.println("PSRAM allocation failed for complete rotation framebuffers.");
    while (true) delay(1000);
  }
  Serial.printf("[DISPLAY] Base rotation: %d degrees clockwise\n", DISPLAY_ROTATION_DEG);
  Serial.printf("[PERF] clipped motion renderer: %s | CPU %u MHz\n",
                AUTO_ROTATION_CLIPPED_RENDER ? "ON" : "OFF",getCpuFrequencyMhz());
#if AUTO_ROTATION_ENABLED
  Serial.println("[IMU] Initializing QMI8658 for smooth screen rotation...");
  const uint8_t whoami = qmi8658_init();
  if (whoami) {
    i2cMutex = xSemaphoreCreateMutex();
    if (!i2cMutex) {
      Serial.println("[IMU] Cannot create I2C mutex; fixed orientation fallback");
    } else {
      autoRotationAvailable = xTaskCreatePinnedToCore(
          motionTask, "motion", 4096, nullptr, 2, nullptr, 0) == pdPASS;
      Serial.printf("[IMU] QMI8658 %u; background task %s at %d Hz\n",
                    whoami, autoRotationAvailable ? "ready" : "FAILED",
                    AUTO_ROTATION_SENSOR_HZ);
    }
  } else {
    Serial.println("[IMU] Sensor not found: falling back to fixed rotation");
  }
#else
  Serial.println("[IMU] Auto-rotation disabled; fixed angle only");
#endif
  ringPixels = (uint16_t *)heap_caps_malloc(
    DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ringPixels) {
    Serial.println("PSRAM allocation failed for smooth full-resolution canvas.");
    while (true) delay(1000);
  }
  disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_buffers(disp, buf1, buf2, BUF_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_add_event_cb(disp, rounder_cb, LV_EVENT_INVALIDATE_AREA, nullptr);
  lv_indev_t *touch = lv_indev_create();
  lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touch, touch_cb);
  createUi();

  WiFi.onEvent(wifiEvent);
  WiFi.mode(WIFI_STA);
  Serial.printf("[WLAN] STA-MAC: %s\n", WiFi.macAddress().c_str());
  WiFi.setAutoReconnect(true);
  Serial.println("[WLAN] Starte Verbindung (2,4 GHz erforderlich)...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // Genau einmal; weitere Versuche macht der Core.
  // Wichtig: keine zusätzlichen WiFi.reconnect()-Aufrufe im loop().
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(2048);
  mqtt.setKeepAlive(45);
}
void loop() {
  maintainNetwork();
  sampleMotion();
  lv_timer_handler();
  updatePhysicalPanel();
  if (millis() - lastUiRefresh > 3000) {
    lastUiRefresh = millis();
    updateUi();
  }
  delay(1);
}
