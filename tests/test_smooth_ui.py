"""Static contract checks for full-resolution smooth LVGL display."""
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]

class SmoothUiTests(unittest.TestCase):
    def test_no_legacy_lvgl_arcs(self):
        main = (ROOT / 'src/main.cpp').read_text()
        self.assertNotIn('lv_arc_create(', main)
        self.assertIn('lv_canvas_set_buffer(ringCanvas, ringCaches[1].pixels, DISPLAY_WIDTH,', main)
        self.assertIn('LV_COLOR_FORMAT_RGB565', main)

    def test_psram_canvas(self):
        main = (ROOT / 'src/main.cpp').read_text()
        self.assertIn('DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t)', main)
        self.assertIn('MALLOC_CAP_SPIRAM', main)
        self.assertIn('drawSmoothRings(provider, s, w)', main)

    def test_larger_fonts_enabled(self):
        config = (ROOT / 'include/lv_conf.h').read_text()
        for item in ('LV_USE_CANVAS 1', 'LV_FONT_MONTSERRAT_16 1',
                     'LV_FONT_MONTSERRAT_22 1', 'LV_FONT_MONTSERRAT_28 1',
                     'LV_FONT_MONTSERRAT_48 1'):
            self.assertIn(item, config)

if __name__ == '__main__':
    unittest.main()
