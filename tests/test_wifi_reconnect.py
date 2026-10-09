"""Offline regression checks for Wi-Fi connect state management."""
import re
from pathlib import Path
import unittest

MAIN = Path(__file__).resolve().parents[1] / 'src' / 'main.cpp'

class WifiReconnectTests(unittest.TestCase):
    def setUp(self):
        self.source = MAIN.read_text(encoding='utf-8')
        # Strip comments so we don't accidentally match documentation.
        self.code = re.sub(r'/\*.*?\*/|//[^\n]*', '', self.source, flags=re.S)

    def test_wifi_begin_only_once(self):
        self.assertEqual(self.code.count('WiFi.begin('), 1)

    def test_no_manual_reconnect_while_connecting(self):
        self.assertNotIn('WiFi.reconnect(', self.code)
        self.assertIn('WiFi.setAutoReconnect(true)', self.code)

    def test_disconnect_reason_is_logged(self):
        self.assertIn('WiFi.onEvent(wifiEvent)', self.code)
        self.assertIn('info.wifi_sta_disconnected.reason', self.code)
        self.assertIn('ARDUINO_EVENT_WIFI_STA_GOT_IP', self.code)
        self.assertIn('ARDUINO_EVENT_WIFI_STA_DISCONNECTED', self.code)

    def test_no_gui_from_wifi_event_thread(self):
        body = self.code.split('static void wifiEvent(', 1)[1].split('static bool validPercent', 1)[0]
        self.assertNotIn('lv_', body)

if __name__ == '__main__':
    unittest.main()
