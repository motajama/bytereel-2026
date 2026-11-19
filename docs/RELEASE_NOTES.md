# byteREEL 2026 release notes

## ISA 2.0 timing and behavior

All instructions use two bytes, `[opcode][argument]`. ISA 2.0 retains all 45
legacy opcode values and adds 14 new instructions. The VM uses a 16-bit PC,
validates even jump targets even when a conditional jump is not taken, and
reports an error when fetching beyond address `0xFE`. SPEED sets a requested
rate of 1–200 instructions per second; it is not a CPU clock or a measured
execution rate. WAIT, AWAIT, and VWAIT add fixed durations to a VM step; STEP
does not wait for instruction timing. Receiving UART byte `0x00` sets `A=0`
and `Z=0`; a timeout preserves A and sets `Z=1`.

## Current status

Candidate `2026.11.19-r3` was built with ESP32 core 2.0.14 and uploaded as an
application-only image to an ESP32-D0WD-V3 with 4 MiB flash on 2026-11-20.
Esptool verified the application write at `0x10000`; the partition table and
filesystem were not written. No serial boot capture or post-upload functional
test was performed. Physical verification of I2C, reset reason, expander
state, filesystem, Wi-Fi transitions, slot save/load, panel, PAL, audio, piezo,
UART, and sustained load remains outstanding. See
[`TEST_REPORT.md`](TEST_REPORT.md) for the exact image digest and test status.
