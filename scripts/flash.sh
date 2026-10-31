#!/bin/bash
# Build and upload the Jukebox sketch via Serial or Over-The-Air (Wi-Fi WebOTA).
# Usage:
#   scripts/flash.sh build
#   scripts/flash.sh upload [serial_port]   (default: first /dev/cu.usbserial-*)
#   scripts/flash.sh ota [ip_or_hostname]   (default: adabox.lan)
set -e
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
SRC="$(cd "$(dirname "$0")/.." && pwd)"
FQBN="esp32:esp32:esp32"   # ESP32 Dev Module, default partition scheme (keeps NVS intact)
BUILD_DIR="/tmp/jukebox-build"
ESPTOOL="$(find ~/Library/Arduino15/packages/esp32 -name "esptool" -type f 2>/dev/null | head -1)"

echo "==> Compiling Jukebox sketch (FQBN: $FQBN)..."
"$CLI" compile --fqbn "$FQBN" --build-path "$BUILD_DIR" "$SRC"

BIN="$BUILD_DIR/jukebox.ino.bin"
ELF="$BUILD_DIR/jukebox.ino.elf"
if [ ! -f "$BIN" ] && [ -f "$ELF" ] && [ -n "$ESPTOOL" ]; then
  "$ESPTOOL" --chip esp32 elf2image --flash-mode dio --flash-freq 80m --flash-size 4MB --elf-sha256-offset 0xb0 -o "$BIN" "$ELF"
fi

ACTION="${1:-build}"
if [ "$ACTION" = "upload" ]; then
  PORT="${2:-$(ls /dev/cu.usbserial-* 2>/dev/null | head -1)}"
  if [ -z "$PORT" ]; then
    echo "Error: No USB serial port found! Connect ESP32 via USB or use 'scripts/flash.sh ota'." >&2
    exit 1
  fi
  echo "==> Uploading via USB serial ($PORT)..."
  "$CLI" upload --fqbn "$FQBN" --port "$PORT" --input-dir "$BUILD_DIR" "$SRC"
elif [ "$ACTION" = "ota" ] || [ "$ACTION" = "web-ota" ]; then
  TARGET="${2:-adabox.lan}"
  if [ ! -f "$BIN" ]; then
    echo "Error: Binary not found at $BIN" >&2
    exit 1
  fi
  echo "==> Uploading over-the-air (WebOTA) to http://$TARGET/update..."
  curl -f -F "update=@$BIN" "http://$TARGET/update"
  echo ""
  echo "==> WebOTA upload completed successfully! AdaBox is rebooting..."
fi
