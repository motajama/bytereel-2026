# byteREEL 2026 acceptance plan

This is a test plan, not a completed report. Mark each result PASS / FAIL /
NOT TESTED and include evidence.

## VM and assembler — host tests

- All 59 opcodes: unique values, two-byte width, valid modes, argument checks,
  and Z behavior.
- `SET FF; ADD 01` → A=00,Z=1; `SET 00; SUB 01` → A=FF,Z=0. CMP preserves A.
  COUNT/LOOP preserve Z. COUNT 00 and LOOP with C=0 error. COUNT 04 executes
  the body four times.
- Fetch at FE reads its argument at FF. After NOP, PC=100 and the next fetch
  errors; HALT at FE is valid. An odd jump target errors even if the
  conditional jump is not taken. Unknown opcode or wrong bank halts at the
  exact address.
- Deterministic RAND at p=00 and FF, separate audio RNG, correct unsigned
  overflow in SEED.
- Fake clock: SPEED 0 → 1000 ms, 4095 → 5 ms. WAIT 10 hex adds 160 ms, AWAIT
  10 adds 320 ms, VWAIT 10 adds 640 ms. Add the interval once. Do not catch up
  missed steps or retain the old 64-instruction JMP 00 watchdog.
- STEP: a new NEXT executes one instruction and skips timed WAIT; pending RX
  must neither skip nor fetch another instruction.
- RX byte 00 → A=00,Z=0; RX FF → A=FF,Z=0. Timeout preserves A and sets Z=1.
  TRYRX on an empty FIFO does not set sticky timeout. UTIME 00 is interruptible
  by STOP. TX success means enqueued; a full FIFO waits until timeout. Check
  overflow and UCLR.
- PPLAY follows real time even during WAIT/RX. Repeated PPLAY resets its
  deadline; PSTOP/HALT/STOP turn piezo off.
- Assembler: forward labels, ORG/DB, all number formats, overlap, bounds, odd
  instruction address, wrong bank, 256-byte output. Raw DB may contain
  unknown opcodes as data. Hex round-trip preserves every byte.
- Assemble manifest examples and compare all 256 bytes with each supplied
  `.bin` image.

## Storage and API — host tests

- EMPTY boot does not write a demo. SAVE/LOAD handle both banks or neither.
  Web SAVE does not modify working RAM.
- Inject faults at each write stage: header, payload, flush, close, readback.
  After reboot, the old or new complete pair remains; never a mixed pair.
- CRC damage to header/generation/payload, truncation, trailing data, invalid
  JSON/ISA/slot, duplicate generation, and ENOSPC produce errors without
  automatic format.
- Corrupt non-empty filesystems are never formatted. A completely erased
  filesystem may be formatted only in STOP. Partition has no overlap, ends at
  0x400000, and requires at least 4 MiB physical flash.
- Two clients edit the same revision: first PUT succeeds, second returns 409;
  local text remains. Panel SAVE racing a GET/PUT behaves the same way.
- PUT during RUN returns 409; BUSY returns 409 and queues no delayed mutation.
  A RUN change during commit must not auto-start when commit finishes.
- Check maximum body, 4096-byte source, 32-byte UTF-8 name, XSS in name/log,
  invalid ID, Content-Type, invalid hex, and source/bytecode mismatch.
- Monitor checks cursor, bootId, gaps, limits, disconnect/reconnect. No
  RUN/STOP/LOAD/RESET/GPIO/TX endpoints.

## Hardware — panel, outputs, and concurrency

- Cold boot with RUN in STOP and RUN: LED init always runs; VM waits for
  STOP→RUN. Open AP and HTTP start. All 19 controls have correct names; both
  directions of one momentary switch cannot trigger an action simultaneously.
- Every LED turns on and off in correct bit order. Check LIVE opcode/argument,
  STEP, SCAN, and MEMORY status. I2C ACK alone is insufficient.
- SAVE 2 s, LOAD 2 s, short RESET vs. CLR 3 s. A mode/slot change during a
  hold cancels it. CLR does not erase slots.
- Pot direction/smoothness, 1–200 requested IPS; record measured IPS
  separately. ADC33 works while Wi-Fi is active.
- Piezo works independently in AUDIO and VIDEO. Test every AV instruction,
  including envelope, LFO, filter, noise, feedback, and glitch.
- GX12 loopback pins 1–2 with bytes 00 and FF; test GND pin 3 separately with
  a multimeter. External receive at 9600 without a web UART bridge.
- AUDIO has no underruns; PAL stays synchronized during normal web reads. One
  hundred VIDEO/AUDIO switches without reboot, work-bank loss, or growing
  heap usage.
- Two clients, monitor, catalogue and slot reads plus the heaviest VIDEO
  FEEDBACK/GLITCH workload for at least 30 minutes, then AUDIO. Record heap,
  minHeap, FPS, dropped frames, underruns, and STOP latency.
- SAVE in STOP while PAL runs: safe pause/resume, no use-after-free of a
  framebuffer, and a readable slot after restart.
- STOP or lost input I2C mutes audio/piezo within 50 ms under normal load. Do
  not create shorts during testing.
- Verify 5V/3V3 supply and POWER LED physically. Wi-Fi load can expose supply
  issues that are absent during panel-only checks.

## Build and release

Perform a clean ESP32 Dev Module build, core 2.0.14, 4 MiB, Huge APP. Record
library versions, `.bin` hash, app size, and static RAM; measure runtime RAM
separately. Release source, web assets, partition, lockfile, examples, tests,
English README, and test report. Do not report physical PASS without a device.
