#!/bin/bash
# Build (and optionally upload) the Jukebox sketch with the arduino-cli bundled in Arduino IDE.
# Usage: scripts/flash.sh [build|upload] [port]   (default: build; port default: first /dev/cu.usbserial-*)
# The sketch folder must be named like the main .ino (Jukebox), so we build from a temp copy.
set -e
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
SRC="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)/Jukebox"; mkdir -p "$TMP"; cp "$SRC"/*.ino "$TMP"/
FQBN="esp32:esp32:esp32"   # ESP32 Dev Module, default partition scheme (keeps NVS intact)
"$CLI" compile --fqbn "$FQBN" --build-path /tmp/jukebox-build "$TMP"
if [ "${1:-build}" = "upload" ]; then
  PORT="${2:-$(ls /dev/cu.usbserial-* 2>/dev/null | head -1)}"
  "$CLI" upload --fqbn "$FQBN" --port "$PORT" --input-dir /tmp/jukebox-build "$TMP"
fi
