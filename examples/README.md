# ISA 2.0 example programs

Each program has an `.asm` source, an exact 256-byte `.bin` image, and an
addressed `.hex` file. `manifest.json` identifies the target bank. Unused
addresses are filled with `00`. These files are test fixtures; firmware must
not automatically overwrite existing device slots with them.

| Program | Bank | Behavior |
|---|---|---|
| `video-glitch` | VIDEO | Moving horizontal bars with visual distortion |
| `piezo-pulse` | VIDEO | Repeated short 1000 Hz beeps on the independent piezo output |
| `uart-echo` | VIDEO | Echoes received bytes; retries RX on timeout; byte `00` is not a timeout |
| `uart-transmit` | VIDEO | Repeatedly sends `55 AA` with a pause |
| `audio-pulses` | AUDIO | Four 440 Hz pulses followed by HALT |
| `uart-audio` | AUDIO | Maps each received byte to `200 + 10 × byte` Hz |

WAIT durations are added to the SPEED instruction interval. For example,
`audio-pulses` does not take exactly 800 ms as in the old emulator because
GATE and LOOP instructions also consume VM steps. Lower requested IPS makes
the difference audible by design.

For UART echo, send data only after RUN clears the old FIFO. Do not combine
echo mode with a physical pin 1–2 loopback: returned bytes would circulate.
The UART transmit example can be used with the loopback for service checks.

`slot-00.json` is a valid pair: VIDEO uses `piezo-pulse`, AUDIO uses
`audio-pulses`. It is prepared for web import. `python3 tools/validate_package.py`
checks the source and golden byte images; it is not a hardware test.
