#!/bin/bash
# Upload a firmware to the Bhoreal.
#   ./upload.sh games          the games build
#   ./upload.sh music          the music build
#   ./upload.sh path/to.hex    any hex file, e.g. ../backup_bhoreal/bhoreal_app.hex
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
case "${1:-}" in
  games|music) HEX="$HERE/build/bhoreal_$1.hex" ;;
  "") echo "usage: ./upload.sh games|music|file.hex"; exit 1 ;;
  *) HEX="$1" ;;
esac
[ -f "$HEX" ] || { echo "not found: $HEX (run ./build.sh first?)"; exit 1; }
AVRDUDE="$HOME/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17"

port=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
[ -n "$port" ] || { echo "Bhoreal not found"; exit 1; }

# Another program holding the port can grab the bootloader mid-write and
# leave a half-written firmware, so refuse to start.
if lsof "$port" >/dev/null 2>&1; then
  echo "Something has $port open (serial monitor, web page, script...). Close it first:"
  lsof "$port"
  exit 1
fi

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
