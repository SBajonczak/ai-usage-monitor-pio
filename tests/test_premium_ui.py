"""Premium UI sanity/regression checks (not a hardware build)."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
CODE = (ROOT / "src/main.cpp").read_text(encoding="utf-8")

class PremiumUiTests(unittest.TestCase):
    def test_rgb565_byte_swap_in_flush(self):
        transform = (ROOT / "include/continuous_rotation.h").read_text(encoding="utf-8")
        self.assertIn("swapped565", transform)
        self.assertIn("(c << 8) | (c >> 8)", transform)
        self.assertIn("fast_rotation::rotate_rgb565", CODE)
        self.assertIn("amoled.drawArea(0, y", CODE)

    def test_reference_ui_palette_and_layout(self):
        self.assertIn("Rgb{27, 221, 237}", CODE) # teal
        self.assertIn("Rgb{217, 64, 232}", CODE) # magenta
        self.assertIn("lv_obj_set_style_radius(weekCard, LV_RADIUS_CIRCLE", CODE)
        self.assertIn('"5H  46% FREI"', CODE)
        self.assertIn('"WOCHE"', CODE)
        self.assertIn("resetLabel", CODE)
        self.assertIn('"TIPPEN ZUM WECHSELN"', CODE)
        self.assertIn('"ONLINE"', CODE)
        self.assertIn('"MQTT OK"', CODE)

    def test_actual_display_and_touch_kept(self):
        self.assertIn("lv_canvas_set_buffer(ringCanvas, ringCaches[1].pixels, DISPLAY_WIDTH,", CODE)
        self.assertIn("lv_canvas_set_buffer(mascotCanvas, mascotPixels,", CODE)
        self.assertIn("Touch_Init();", CODE)
        self.assertIn("mqtt.subscribe(MQTT_TOPIC, 1)", CODE)
        self.assertIn("lv_obj_add_event_cb(tapLayer, onScreenEvent, LV_EVENT_CLICKED", CODE)

    def test_no_conflicting_arduino_macro(self):
        self.assertNotRegex(CODE, r"\b(?:constexpr|const)\s+float\s+TWO_PI\b")

    def test_rgb565_swap_math(self):
        # Sent low-byte-first on ESP32, panel expects high-byte-first RGB565.
        def pack(r,g,b): return ((r>>3)<<11) | ((g>>2)<<5) | (b>>3)
        for color in ((7,11,19), (27,221,237), (217,64,232)):
            native = pack(*color)
            swapped = ((native << 8) | (native >> 8)) & 0xFFFF
            self.assertEqual(swapped.to_bytes(2, "little"), native.to_bytes(2,"big"))

if __name__ == "__main__": unittest.main()
