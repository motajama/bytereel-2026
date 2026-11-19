#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT="$ROOT/firmware/build-littlefs-test"
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --libraries "$ROOT/firmware/libraries" \
  --output-dir "$OUT" \
  --build-property upload.maximum_size=3145728 \
  --build-property build.flash_size=4MB \
  --build-property "compiler.cpp.extra_flags=-I$ROOT/firmware/bytereel_2026" \
  "$ROOT/littlefs_fault_test"
