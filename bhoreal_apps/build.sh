#!/bin/bash
# Compile the Bhoreal firmwares with the AVR toolchain left by the Arduino IDE
# (~/Library/Arduino15), without needing arduino-cli.
#
#   ./build.sh          build both firmwares
#   ./build.sh games    build only the games firmware
#   ./build.sh music    build only the music firmware
#
# Output: build/bhoreal_games.hex and build/bhoreal_music.hex
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
A15="$HOME/Library/Arduino15/packages/arduino"
CORE="$A15/hardware/avr/1.8.6"
BIN="$A15/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin"
OUT="$HERE/build"

COMMON=(BhorealHw.cpp UsbMidi.cpp Draw.cpp bhoreal_apps.ino)
GAMES=(TalpsApp.cpp SimonApp.cpp MemoryApp.cpp FroggerApp.cpp SnakeApp.cpp Connect4App.cpp DuelApp.cpp PongApp.cpp)
MUSIC=(SeqApp.cpp BallsApp.cpp)

INC=(-I"$CORE/cores/arduino" -I"$CORE/variants/leonardo"
     -I"$CORE/libraries/Wire/src" -I"$CORE/libraries/EEPROM/src" -I"$HERE")
CFLAGS=(-c -g -Os -std=gnu11 -ffunction-sections -fdata-sections -flto -fno-fat-lto-objects)
CXXFLAGS=(-c -g -Os -std=gnu++11 -fpermissive -fno-exceptions -ffunction-sections
          -fdata-sections -fno-threadsafe-statics -Wno-error=narrowing -flto)

compile() {  # src obj extra-flags...
  local src="$1" obj="$2"; shift 2
  case "$src" in
    *.c) "$BIN/avr-gcc" "${CFLAGS[@]}" "$@" "${DEFS[@]}" "${INC[@]}" "$src" -o "$obj" ;;
    *.S) "$BIN/avr-gcc" -c -g -x assembler-with-cpp -flto "${DEFS[@]}" "${INC[@]}" "$src" -o "$obj" ;;
    *)   "$BIN/avr-g++" "${CXXFLAGS[@]}" "$@" "${DEFS[@]}" "${INC[@]}" -x c++ "$src" -o "$obj" ;;
  esac
  objs+=("$obj")
}

build() {  # target
  local target="$1" define product apps
  case "$target" in
    games) define=BHOREAL_GAMES; product="Bhoreal Jocs"; apps=("${GAMES[@]}") ;;
    music) define=BHOREAL_MUSIC; product="Bhoreal Musica"; apps=("${MUSIC[@]}") ;;
    *) echo "unknown target: $target (use games or music)"; exit 1 ;;
  esac
  local dir="$OUT/$target"
  mkdir -p "$dir"
  DEFS=(-mmcu=atmega32u4 -DF_CPU=16000000L -DARDUINO=10819 -DARDUINO_AVR_LEONARDO
        -DARDUINO_ARCH_AVR -DUSB_VID=0x2341 -DUSB_PID=0x8036 -D$define
        '-DUSB_MANUFACTURER="Bhoreal"' "-DUSB_PRODUCT=\"$product\"")
  objs=()
  for f in "$CORE"/cores/arduino/*.{c,cpp,S} "$CORE"/libraries/Wire/src/Wire.cpp \
           "$CORE"/libraries/Wire/src/utility/twi.c; do
    compile "$f" "$dir/core_$(basename "$f").o" -w
  done
  for f in "${COMMON[@]}" "${apps[@]}"; do
    compile "$HERE/$f" "$dir/$f.o" -Wall
  done

  "$BIN/avr-gcc" -w -Os -g -flto -fuse-linker-plugin -Wl,--gc-sections -mmcu=atmega32u4 \
    -o "$dir/bhoreal_$target.elf" "${objs[@]}" -lm
  "$BIN/avr-objcopy" -O ihex -R .eeprom "$dir/bhoreal_$target.elf" "$OUT/bhoreal_$target.hex"
  echo "== $target"
  "$BIN/avr-size" -A "$dir/bhoreal_$target.elf" | grep -E "^\.(text|data|bss)"
  echo "OK: $OUT/bhoreal_$target.hex"
}

if [ $# -eq 0 ]; then
  build games
  build music
else
  build "$1"
fi
