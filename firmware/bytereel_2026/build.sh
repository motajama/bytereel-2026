#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
python3 "$ROOT/tools/generate_demos.py" --check
if [ "${BR_AUDIO_DIAGNOSTIC:-0}" = 1 ]; then
  arduino-cli compile --fqbn esp32:esp32:esp32 \
    --libraries "$ROOT/firmware/libraries" \
    --output-dir "$ROOT/firmware/build/audio-diagnostic" \
    --build-property upload.maximum_size=3145728 \
    --build-property build.flash_size=4MB \
    --build-property compiler.cpp.extra_flags=-DBR_AUDIO_DIAGNOSTIC=1 \
    "$ROOT/firmware/bytereel_2026"
else
  arduino-cli compile --fqbn esp32:esp32:esp32 \
    --libraries "$ROOT/firmware/libraries" \
    --output-dir "$ROOT/firmware/build" \
    --build-property upload.maximum_size=3145728 \
    --build-property build.flash_size=4MB \
    "$ROOT/firmware/bytereel_2026"
fi
