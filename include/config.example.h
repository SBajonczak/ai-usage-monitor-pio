#pragma once
// Copy this file to include/config.h and fill in your own values.
// Do not commit config.h. No Claude/Codex authentication belongs on the ESP32.
#define WIFI_SSID "MEIN-WLAN"
#define WIFI_PASSWORD "WLAN-PASSWORT"
#define MQTT_HOST "192.168.50.48"  // Change to the broker address (example only).
#define MQTT_PORT 1883
#define MQTT_USER "mqtt-user"
#define MQTT_PASSWORD "mqtt-passwort"
#define MQTT_TOPIC "ai-monitor/state"
