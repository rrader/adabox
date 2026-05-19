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
| Next button | GPIO 13 → GND |
| Prev button | GPIO 14 → GND |
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

Hold Play for 5 seconds to toggle between MUSIC and OTHER browse modes (orange flash = MUSIC, purple flash = OTHER).

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

## Libraries

| Library | Source |
|---|---|
| **FastLED** ≥ 3.6 | Arduino Library Manager |

No library needed for the DY-SV5W — it speaks raw UART at 9600 baud, handled directly by the ESP32's `HardwareSerial` (`Serial2`).

## Arduino IDE setup

1. Add ESP32 board support: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Board: **ESP32 Dev Module** (or your specific variant)
3. Install **FastLED** via Library Manager
4. Upload `Jukebox.ino`

## DY-SV5W DIP switch

Set the module to **UART control mode**: CON3=1, CON2=0, CON1=0.

## Adjusting volume

Change the value in `setup()`:

```cpp
playerVolume(25);  // 0–30
```
