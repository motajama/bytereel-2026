#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --libraries "$ROOT/firmware/libraries" \
  --output-dir "$ROOT/firmware/build" \
  --build-property upload.maximum_size=3145728 \
  --build-property build.flash_size=4MB \
  "$ROOT/firmware/bytereel_2026"
