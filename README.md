# AI Usage Monitor – PlatformIO / ESP32-S3 Waveshare 1.43" AMOLED – Smooth UI v1.3 (WLAN-Fix)

Ein **PlatformIO-C++-Projekt** für das **Waveshare ESP32-S3-Touch-AMOLED-1.43** mit **466 × 466 Pixeln**, QSPI-Display **SH8601/CO5300** und Touch **FT3168**.

## Funktionen

- Zeigt **Claude** und **Codex** als zwei umschaltbare LVGL-Seiten.
- Hochaufgelöste, weich geglättete Fortschrittsringe auf dem nativen **466×466-RGB565-Canvas** (ohne Skalierung); 5-Stunden-Restkapazität außen und Wochen-Restkapazität innen.
- Größere Montserrat-Schriften (16–48 px) und überarbeitete Abstände für das kleine AMOLED.
- Einmal auf das Display **tippen**, um zwischen Claude / Codex zu wechseln.
- Empfängt Werte über WLAN per **MQTT** (`ai-monitor/state`), behält die letzten empfangenen Werte.
- **DEMO** solange ein Anbieter noch keine echten Werte geliefert hat; **LIVE** wenn Werte eingetroffen sind; **DATEN VERALTET**, wenn 20 Minuten keine neue Nachricht kam.
- **Keine** Claude-/OpenAI-Passwörter oder OAuth-Tokens auf dem ESP32.

## 1. Projekt öffnen

1. ZIP entpacken und den Ordner mit **VS Code → PlatformIO → Open Project** öffnen.
2. `include/config.h` bearbeiten: WLAN, tatsächliche IP des MQTT-Brokers (nicht automatisch Home Assistant), Benutzer und Passwort. Die enthaltenen Werte sind **Platzhalter**, das Projekt startet auch ohne eingerichteten Broker im Demo-Modus.
3. USB-C mit dem Waveshare verbinden.
4. In VS Code: **PlatformIO: Build** und anschließend **Upload**. Alternativ:

   ```sh
   pio run -e waveshare_143
   pio run -e waveshare_143 -t upload
   pio device monitor -b 115200
   ```

Das Pre-Build-Skript verwendet `env.subst("$PROJECT_DIR")` und benötigt im PlatformIO/SCons-Kontext kein `__file__`. Ein manueller Aufruf mit `python scripts/fetch_waveshare.py` bleibt möglich.

Beim **allerersten Build** lädt PlatformIO den ESP32-Core, LVGL, ArduinoJson, PubSubClient sowie über `scripts/fetch_waveshare.py` die neun Hardwaretreiber-Dateien des [Waveshare-LVGL9-Basisprojekts](https://github.com/thelastoutpostworkshop/waveshare_esp32s3_1.43_amoled_lvgl9). Die Dateien werden unter `src/vendor/` zwischengespeichert. Danach sind die Treiber auch für Offline-Builds verfügbar, wenn die PlatformIO-Pakete bereits installiert sind.

**Warum werden die Treiber nachgeladen?** Die proprietäts- und boardnahen Initialisierungssequenzen stammen aus dem funktionierenden Open-Source-Projekt und werden hier nicht als veränderte Kopie verteilt. Damit werden QSPI-Pins und Panel-Kommandos nicht geraten. Die Originalquellen stehen unter MIT-Lizenz und sind beim Autor verfügbar.

## 2. Home Assistant: MQTT-Test

**Entwicklerwerkzeuge → Aktionen → `mqtt.publish`**:

```yaml
action: mqtt.publish
data:
  topic: ai-monitor/state
  qos: 1
  retain: true
  payload: >-
    {"claude":{"session_remaining":72,"weekly_remaining":57},"codex":{"session_remaining":46,"weekly_remaining":35}}
```

Beispielwerte! `session_remaining` und `weekly_remaining` sind **Restkapazität in Prozent** (0–100), nicht die prozentuale Nutzung. `-1` bedeutet unbekannt.

Sobald du echte Sensoren hast, kannst du die Vorlage in `homeassistant/automation.yaml` übernehmen und die Entity-IDs durch deine tatsächlichen Sensoren ersetzen. Falls Claude noch nicht eingerichtet ist, bleibt Claude im DEMO-Modus. Codex kann trotzdem bereits LIVE-Daten erhalten.

## 3. Dateien

```text
ai-usage-monitor-pio/
├── platformio.ini                 # ESP32-S3, Arduino 3.3.0, LVGL 9.2.2
├── partitions.csv                 # 16-MB-Flash / zwei OTA-Partitionen
├── include/
│   ├── config.h                   # lokale Einstellungen (in .gitignore)
│   ├── config.example.h           # Vorlage
│   └── lv_conf.h                  # LVGL-Konfiguration
├── src/
│   ├── main.cpp                   # Display, UI, WLAN, MQTT
│   └── vendor/                    # Boardtreiber beim ersten Build automatisch
├── scripts/
│   └── fetch_waveshare.py         # Treiber-Bootstrap
└── homeassistant/
    └── automation.yaml            # Beispiel-HA-Automation
```

## Technische Hinweise

- Hardware: ESP32-S3R8, 16 MB Flash, 8 MB OPI PSRAM; Pin-Belegung aus dem [Waveshare-Board](https://www.waveshare.com/esp32-s3-touch-amoled-1.43.htm).
- PlatformIO verwendet **pioarduino 55.03.30 / Arduino Core 3.3.0**. Der offizielle PlatformIO-ESP32-Core 2.x ist für diese LVGL9-Hardwarebasis **nicht** der passende Ersatz.
- Alle GUI-Aufrufe erfolgen im Arduino-`loop()`-Kontext; es gibt keinen zweiten Task, der gleichzeitig LVGL verändert.
- MQTT ist in dieser ersten Version **unverschlüsselt im lokalen Netz**. Den Broker nur im vertrauenswürdigen Netz erreichbar machen und ein eigenes MQTT-Konto verwenden.
- `192.168.50.48` in der Konfigurationsdatei ist lediglich ein Beispiel. Setze die reale Broker-IP ein.
- Die Anzeige zeigt vorerst **keine Reset-Zeitpunkte**. Die erste Version verarbeitet nur Prozentwerte.
- Das ist eine **ungetestete Hardware-Portierung**: Hier stand weder ein physisches Display noch ein funktionierender PlatformIO-ESP32-Build mit externem Paketdownload zur Verfügung. Bei Buildfehlern bitte die erste Fehlermeldung aus dem Terminal verwenden.

## Fehlerdiagnose

- **`amoled.h` fehlt**: `python scripts/fetch_waveshare.py` ausführen. Falls GitHub blockiert ist, die neun Dateien aus dem Upstream-Repo manuell nach `src/vendor` kopieren.
- **LVGL kann `lv_conf.h` nicht finden**: kontrolliere `-DLV_CONF_INCLUDE_SIMPLE` und `-Iinclude` in `platformio.ini`.
- **Neustart wegen PSRAM**: prüfen, ob in der Plattform `board_build.arduino.memory_type = qio_opi` und `board_build.psram_type = opi` gesetzt ist.
- **Display schwarz**: USB-Stromversorgung, Serial Monitor, exakten Boardnamen kontrollieren; vor Firmware-Änderungen sicherstellen, dass die Waveshare-Hardwarevorlage für die Platinenrevision passt.
- **Nur DEMO**: Wi-Fi-/MQTT-Zugangsdaten, Broker-IP und MQTT-Topic prüfen. Im Serial Monitor wird bei MQTT-Verbindungsfehlern ein Statuscode ausgegeben.
- **Claude weiterhin DEMO**: Claude OAuth-Integration kann unabhängig davon noch fehlen. Die Firmware ist nicht dafür zuständig, die OAuth-Anmeldung durchzuführen.

## Smooth UI v1.2: Unterschied zur ursprünglichen Version

- LVGL-Arcs durch selbst gezeichnete Ringe mit antialiasierter Pixelabdeckung ersetzt.
- Kein Upscaling von 240 × 240 auf 466 × 466; Canvas und LVGL-Display verwenden dieselben **466 × 466** Pixel.
- Ring-Canvas benötigt ca. **424 KiB PSRAM** zusätzlich.
- Der Canvas wird nur bei Anbieterwechsel oder geänderten Prozenten neu berechnet, nicht bei jedem MQTT-/UI-Tick.
- Größere Montserrat-Schriften verringern die visuelle Grobpixeligkeit.
- MQTT-Payload, Topics und `include/config.h` sind zu v1.1 kompatibel.
- Bei Austausch in einem vorhandenen Projekt: **`src/main.cpp` und `include/lv_conf.h`** ersetzen; Einstellungen behalten.
- Wichtig: Ein physischer ESP32-Build und die optische Darstellung wurden hier nicht auf Hardware verifiziert.

## WLAN-Fix v1.3

- Der Arduino-Core hat mit `WiFi.setAutoReconnect(true)` bereits eine Reconnect-Logik. Der bisherige zusätzliche Aufruf von `WiFi.reconnect()` alle zehn Sekunden wurde **entfernt**, da er während eines laufenden Verbindungsversuchs `wifi:sta is connecting, return error` auslösen kann.
- Im seriellen Monitor werden nun die relevanten Ereignisse und der `DISCONNECTED`-Grund angezeigt.
- Gängige Codes: `201` = `NO_AP_FOUND` (SSID/Funknetz), `202` = `AUTH_FAIL` (WPA/Passwort), `203` = `ASSOC_FAIL` (Anmeldung abgelehnt), `204` = `HANDSHAKE_TIMEOUT` (WPA/Signal).
- ESP32-S3 unterstützt **nur 2,4-GHz-WLAN**. Bei UniFi kontrollieren: passender SSID-Funkbereich, WPA2-Personal oder kompatibler WPA2/WPA3-Mischmodus, nicht ausschließlich WPA3/Enterprise, VLAN-/ACL-Regeln und ggf. PMF-Einstellungen.
- `WLAN Verbunden. IP:` bedeutet, dass WLAN und DHCP funktionieren. Falls anschließend `MQTT connect failed` erscheint, ist das ein **separates Broker-/Netzwerkproblem**.
- Nur die Datei `src/main.cpp` muss für diesen Fix in einem bereits installierten v1.2-Projekt ersetzt werden. Deine `include/config.h` mit WLAN-Zugangsdaten bleibt erhalten.
