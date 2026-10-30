#!/bin/bash
# Upload a hex file to the Bhoreal (default: the apps build).
# To restore the original firmware:
#   ./upload.sh ../backup_bhoreal/bhoreal_app.hex
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
HEX="${1:-$HERE/build/bhoreal_apps.hex}"
AVRDUDE="$HOME/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17"

port=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
[ -n "$port" ] || { echo "Bhoreal not found"; exit 1; }

# Opening the port at 1200 baud makes the board reboot into its bootloader.
stty -f "$port" 1200 || true
for _ in $(seq 1 40); do [ -e "$port" ] || break; sleep 0.1; done
for _ in $(seq 1 80); do
  boot=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
  [ -n "$boot" ] && break
  sleep 0.1
done
[ -n "$boot" ] || { echo "Bootloader port not found"; exit 1; }
sleep 0.5

"$AVRDUDE/bin/avrdude" -C "$AVRDUDE/etc/avrdude.conf" -p atmega32u4 -c avr109 \
  -P "$boot" -b 57600 -D -U "flash:w:$HEX:i"
