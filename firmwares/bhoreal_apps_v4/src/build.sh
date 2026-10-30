#!/bin/bash
# Compile the Bhoreal apps firmware with the AVR toolchain left by the Arduino IDE
# (~/Library/Arduino15), without needing arduino-cli.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
A15="$HOME/Library/Arduino15/packages/arduino"
CORE="$A15/hardware/avr/1.8.6"
BIN="$A15/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin"
OUT="$HERE/build"
mkdir -p "$OUT"

DEFS=(-mmcu=atmega32u4 -DF_CPU=16000000L -DARDUINO=10819 -DARDUINO_AVR_LEONARDO
      -DARDUINO_ARCH_AVR -DUSB_VID=0x2341 -DUSB_PID=0x8036
      '-DUSB_MANUFACTURER="Bhoreal"' '-DUSB_PRODUCT="Bhoreal"')
INC=(-I"$CORE/cores/arduino" -I"$CORE/variants/leonardo"
     -I"$CORE/libraries/Wire/src" -I"$CORE/libraries/EEPROM/src" -I"$HERE")
CFLAGS=(-c -g -Os -std=gnu11 -ffunction-sections -fdata-sections -flto -fno-fat-lto-objects)
CXXFLAGS=(-c -g -Os -std=gnu++11 -fpermissive -fno-exceptions -ffunction-sections
          -fdata-sections -fno-threadsafe-statics -Wno-error=narrowing -flto)

objs=()
compile() {  # src obj extra-flags...
  local src="$1" obj="$2"; shift 2
  case "$src" in
    *.c) "$BIN/avr-gcc" "${CFLAGS[@]}" "$@" "${DEFS[@]}" "${INC[@]}" "$src" -o "$obj" ;;
    *.S) "$BIN/avr-gcc" -c -g -x assembler-with-cpp -flto "${DEFS[@]}" "${INC[@]}" "$src" -o "$obj" ;;
    *)   "$BIN/avr-g++" "${CXXFLAGS[@]}" "$@" "${DEFS[@]}" "${INC[@]}" -x c++ "$src" -o "$obj" ;;
  esac
  objs+=("$obj")
}

for f in "$CORE"/cores/arduino/*.{c,cpp,S} "$CORE"/libraries/Wire/src/Wire.cpp \
         "$CORE"/libraries/Wire/src/utility/twi.c; do
  compile "$f" "$OUT/core_$(basename "$f").o" -w
done
for f in "$HERE"/*.cpp "$HERE"/bhoreal_apps.ino; do
  compile "$f" "$OUT/$(basename "$f").o" -Wall
done

"$BIN/avr-gcc" -w -Os -g -flto -fuse-linker-plugin -Wl,--gc-sections -mmcu=atmega32u4 \
  -o "$OUT/bhoreal_apps.elf" "${objs[@]}" -lm
"$BIN/avr-objcopy" -O ihex -R .eeprom "$OUT/bhoreal_apps.elf" "$OUT/bhoreal_apps.hex"
"$BIN/avr-size" -A "$OUT/bhoreal_apps.elf" | grep -E "^\.(text|data|bss)"
echo "OK: $OUT/bhoreal_apps.hex"
