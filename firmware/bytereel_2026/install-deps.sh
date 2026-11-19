#!/bin/sh
set -eu

# Install the exact toolchain/library versions in dependencies.lock.
arduino-cli core update-index
arduino-cli core install esp32:esp32@2.0.14
arduino-cli lib install "Adafruit GFX Library@1.11.9"
arduino-cli lib install "Adafruit BusIO@1.16.1"

echo "Local composite-video library is supplied in firmware/libraries/."
echo "Run: sh firmware/bytereel_2026/build.sh"
