"""Lightweight offline structural checks, no ESP-IDF toolchain required."""
from pathlib import Path
import configparser
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ProjectStructureTests(unittest.TestCase):
    def test_expected_files(self):
        for entry in ('platformio.ini', 'src/main.cpp', 'include/lv_conf.h',
                      'include/config.h', 'homeassistant/automation.yaml',
                      'scripts/fetch_waveshare.py', 'partitions.csv'):
            self.assertTrue((ROOT / entry).is_file(), entry)

    def test_pio_config(self):
        cfg = configparser.ConfigParser(interpolation=None)
        cfg.read(ROOT / 'platformio.ini')
        env = cfg['env:waveshare_143']
        self.assertIn('framework', env)
        self.assertEqual(env['framework'], 'arduino')
        self.assertIn('pre:scripts/fetch_waveshare.py', env['extra_scripts'])
        self.assertIn('lvgl/lvgl@9.2.2', env['lib_deps'])

    def test_mqtt_protocol(self):
        main = (ROOT / 'src/main.cpp').read_text()
        self.assertIn('session_remaining', main)
        self.assertIn('weekly_remaining', main)
        self.assertIn('MQTT_TOPIC', main)
        self.assertIn('LV_DISPLAY_RENDER_MODE_PARTIAL', main)


if __name__ == '__main__':
    unittest.main()
