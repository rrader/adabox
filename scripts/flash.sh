#!/bin/bash
# Build (and optionally upload) the Jukebox sketch with the arduino-cli bundled in Arduino IDE.
# Usage: scripts/flash.sh [build|upload] [port]   (default: build; port default: first /dev/cu.usbserial-*)
# The sketch folder name must match the main .ino (projects/jukebox/jukebox.ino), so we build in place.
set -e
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
SRC="$(cd "$(dirname "$0")/.." && pwd)"
FQBN="esp32:esp32:esp32"   # ESP32 Dev Module, default partition scheme (keeps NVS intact)
"$CLI" compile --fqbn "$FQBN" --build-path /tmp/jukebox-build "$SRC"
if [ "${1:-build}" = "upload" ]; then
  PORT="${2:-$(ls /dev/cu.usbserial-* 2>/dev/null | head -1)}"
  "$CLI" upload --fqbn "$FQBN" --port "$PORT" --input-dir /tmp/jukebox-build "$SRC"
fi
