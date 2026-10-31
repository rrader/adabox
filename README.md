# Jukebox

My own take on the [ESPuino](https://github.com/biologist79/ESPuino) / [Tonuino](https://www.tonuino.de) / Toniebox concept — an RFID-controlled music box where you tap a card to play music. Built on ESP32 with a DY-SV5W audio module and a 16-LED WS2812 ring, kept deliberately simple: no WiFi, no app, no cloud — just cards and buttons.

## Hardware

- ESP32 (any devkit)
- DY-SV5W MP3 player module (SV5W)
- WS2812 16-LED ring (SKV225)
- 3 momentary push buttons
- FAT32 micro SD card with tracks named `00001.mp3` … `00024.mp3` in the root

## Wiring

| Component | ESP32 pin |
|---|---|
| DY-SV5W RXD | GPIO 17 |
| DY-SV5W TXD | GPIO 16 |
| DY-SV5W BUSY | GPIO 15 |
| DY-SV5W +5V | 5V |
| DY-SV5W GND | GND |
| WS2812 DATA | GPIO 4 |
| WS2812 +5V | 5V |
| WS2812 GND | GND |
| Play/Stop button | GPIO 12 → GND |
| Next button | GPIO 14 → GND |
| Prev button | GPIO 13 → GND |
| KY-040 CLK | GPIO 18 |
| KY-040 DT | GPIO 19 |
| KY-040 + | 3.3V |
| KY-040 GND | GND |
| RC522 SCK | GPIO 25 |
| RC522 MISO | GPIO 26 |
| RC522 MOSI | GPIO 27 |
| RC522 SDA (SS) | GPIO 5 |
| RC522 RST | GPIO 22 |
| RC522 3.3V | 3.3V |
| RC522 GND | GND |

Buttons use the internal pull-up — no external resistors needed.

## SD Card preparation

Format the card as FAT32 (single partition, MBR). Use two folders:

```
MUSIC/
  00001.mp3    ← browsed with Next/Prev in MUSIC mode, auto-advances
  00002.mp3
  ...
OTHER/
  00001.mp3    ← browsed with Next/Prev in OTHER mode, no auto-advance
  00002.mp3
  ...
```

Five-digit zero-padded filenames required. Track counts are auto-detected at boot.

RFID cards store the **full file path** (e.g. `/MUSIC/00003.mp3`), not a track number. To program a card: navigate to the desired track with Next/Prev, then hold Next+Prev and tap the card.

While stopped, hold Next for 5 seconds to toggle between MUSIC and OTHER browse modes (orange flash = MUSIC, purple flash = OTHER).

## LED ring behavior

| State | Animation |
|---|---|
| Stopped / idle | Slow blue breathing |
| Playing | Rainbow spinner rotating around the ring |
| Play pressed | Green burst flash |
| Stop pressed | Red burst flash |
| Next pressed | Clockwise cyan comet sweep |
| Prev pressed | Counter-clockwise yellow comet sweep |
| Track ends | White burst flash, auto-advances to next track |
| Volume changed | Blue arc overlay (1..16 LEDs, 1.2s) |
| LED level changed | Amber arc overlay (0..16 LEDs, 1.2s) |
| Next / Prev / Auto-advance | Track number dots overlay (2s): 1..16 dots in ring color; next 16s fill another ring with the next contrasting color (up to 160 tracks across 10 colors) |

## Libraries

| Library | Source |
|---|---|
| **FastLED** ≥ 3.6 | Arduino Library Manager |
| **MFRC522** | Arduino Library Manager |

No library needed for the DY-SV5W — it speaks raw UART at 9600 baud, handled directly by the ESP32's `HardwareSerial` (`Serial2`). For the UART protocol details (especially the quirky path encoding for command `0x08`, where `/MUSIC/00001.mp3` must be sent as `/MUSIC*/00001*MP3`), the [SnijderC/dyplayer](https://github.com/SnijderC/dyplayer) library is a useful reference implementation.

## Arduino IDE setup

1. Add ESP32 board support: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Board: **ESP32 Dev Module** (or your specific variant)
3. Install **FastLED** via Library Manager
4. Upload `jukebox.ino`

## DY-SV5W DIP switch

Set the module to **UART control mode**: CON3=1, CON2=0, CON1=0.

## Controls

| Action | Result |
|---|---|
| Play/Stop | Stop, or play the current track of the current mode |
| Next / Prev | Next / previous track (OTHER: resumes from the saved position; Prev after 10 s restarts the track) |
| Encoder | Volume (2–14 of the module's 0–30 scale) |
| Play held + encoder | Seek ±5 s per tick |
| Prev held + encoder | LED brightness, level 0 (off) – 10 (max) |
| Next held 5 s (stopped) | Toggle MUSIC / OTHER browse mode |
| Next + Prev held, then tap a card | Write the current track path to the card |
| Play + Next + Prev held 15 s | Factory reset: erase saved state, LEDs blink red, reboot |

## Saved state (NVS)

Survives power-off and re-flashing (as long as "Erase All Flash" is off and the partition scheme is unchanged): last mode, last track per mode, volume, LED level, Wi-Fi credentials, and the playback position of every OTHER track (resume point). After 10 minutes of inactivity the ESP32 enters deep sleep; press Play to wake it.

## Wi-Fi, Web UI & OTA Updates

AdaBox includes Wi-Fi support with a captive portal for setup without hardcoded credentials:
- **Captive Portal:** If no Wi-Fi credentials are stored, AdaBox starts a Wi-Fi hotspot `AdaBox-Setup` (192.168.4.1) with a captive portal to select and save Wi-Fi network details into NVS.
- **Web UI:** Accessible on the local network at `http://adabox.lan/` (or `http://adabox.local/`) to view status, mode, track, volume, and LED level.
- **JSON API:** `GET http://adabox.lan/info` returns pure JSON for home automation.
- **OTA Updates:** Web-based firmware update page at `http://adabox.lan/update` (upload compiled `.bin` via browser or `scripts/flash.sh ota adabox.lan`).

## Home Assistant Integration

AdaBox integrates directly into Home Assistant via REST sensors and Lovelace maintenance cards:
- Full setup instructions and configuration templates are located in [`home-assistant/`](home-assistant/).
- Monitored entities: `binary_sensor.adabox_online`, `sensor.adabox_firmware_version`, `sensor.adabox_status`, `sensor.adabox_mode`, `sensor.adabox_track`, `sensor.adabox_volume`, `sensor.adabox_led_level`, and `sensor.adabox_wi_fi_rssi`.

## Flashing

`scripts/flash.sh build` compiles, `scripts/flash.sh upload [port]` also uploads. `scripts/serial_log.py [seconds] [port]` resets via RTS and prints the serial console (uses the arduino-cli bundled with Arduino IDE, board `esp32:esp32:esp32`). Libraries: FastLED, MFRC522.

## Adjusting volume

Limits and default are `VOLUME_MIN`, `VOLUME_MAX`, `VOLUME_DEFAULT` in `jukebox.ino` (the last used volume is restored on boot).
