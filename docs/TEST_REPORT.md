# byteREEL 2026 test report

## Current deployed application — 2026-11-19-r3

- Wi-Fi lifecycle is requested only on a debounced EDIT/MEMORY transition.
  Radio start/stop runs on a separate FreeRTOS task; HTTP route setup/start and
  stop stay in the main loop, and the main loop reads the panel before servicing
  HTTP. Boot in EDIT leaves the radio/server off. Route registration is
  guarded and retained across server stop/start. The network transition helper
  passed 100 host EDIT↔MEMORY cycles while a RAM sentinel and log entry remained
  unchanged. This is state-helper evidence, not repeated hardware AP evidence.
- Persistence audit found that `BrLittleFsStore::read()` returned the same
  `-1` as “missing file” for an existing file larger than the candidate buffer.
  This made a damaged/oversize generation indistinguishable from a genuinely
  absent candidate. It now returns an I/O error (`READ_OVERSIZE`), which blocks
  commit instead of treating that generation as empty. The host commit tests
  cover first commit to an empty pair, repeat SAVE, revision conflict, verified
  readback/CRC, and interrupted write recovery. The earlier hardware report
  contained no commit-stage trace, so this proven source defect cannot be
  attributed as the sole cause of the specific earlier empty-slot attempt.
  The firmware now reports code, phase, and adapter reason for subsequent
  failures; success is logged only after readback and CRC verification.
- Panel SAVE and web SAVE share `slotWrite()` and `br_commit_slot()`; panel
  selection and hold/release behavior are tested using the same pure panel and
  commit helpers. Frontend test covers opening an empty slot, first save,
  repeated save/revision, reload, and delete. Its stateful HTTP adapter is not
  the Arduino `WebServer` runtime; the real handler is compile-checked, not
  exercised over hardware HTTP.
- Slow LED phases are nonblocking: MEMORY and EDIT preview use shared 1000 ms
  ON/OFF transitions; error pulses use six 100 ms phases (three flashes).
  Host tests cover transitions, re-entry lit phase, delayed loop catch-up, and
  restoration to normal display.
- `sh host-tests/run.sh`: **PASS**. `sh firmware/bytereel_2026/build.sh`:
  **PASS**, core 2.0.14 / current partition, sketch 963,653 B, static RAM
  107,272 B. Application SHA-256:
  `8700869a542a51193ba9f972979fe0f08a1eddb2d5d6ab13b35204b2133a8349`.
  Uploaded application-only to `/dev/cu.usbserial-0001` at `0x10000` on
  2026-11-20. Esptool 4.5.1 identified ESP32-D0WD-V3 revision 3.1 and verified
  the write hash. No bootloader, partition table, or filesystem image was
  written; no erase command or flash read was performed. No serial boot capture
  or physical acceptance test followed this upload.

## Previous unflashed candidate — 2026-11-19-r2

- LED mapping: one centralized word mapper reverses only the upper/address
  byte and leaves the lower/data byte in its former order. The target is
  A7…A0 left-to-right above and D7…D0 below; switch decoding and RAM bytes are
  unchanged. Host vectors cover `01`, `80`, `55`, and `AA` independently on
  both rows.
- Smooth breathing is **not implemented**. At 10 kHz, a two-byte I2C
  transaction costs about 27 clocks / 2.7 ms. Input reads every 5 ms use
  roughly 54% of ideal bus capacity, leaving at most about 170 output writes/s
  before other work. The board package does not identify the expander part;
  the observed raw-port interface has no usable PWM control. A smooth shared
  multi-bit PWM envelope cannot be promised within this bus budget while
  preserving panel/STOP latency. Ordinary indicators are steady; only the
  separate three-flash error signal changes over time. Exact silicon and
  perceived brightness still need physical confirmation.
- Web defects found in the previous source: monitor/status catches discarded
  fetch failures, slot requests did not consistently check HTTP status, all
  404 slot responses were treated as empty (including corrupt data), and the
  log endpoint advertised the newest global sequence even when response-size
  clipping had omitted entries, skipping those entries on the next poll. Its
  dynamic route also had no DELETE handler. The UI now reports operation,
  HTTP status and API reason, distinguishes `SLOT_EMPTY`, refreshes slot cards,
  uses a shared tested route matcher including DELETE, returns the last
  actually emitted log sequence, and sends no-cache headers plus firmware/UI
  version identifiers. The source-level causes and handler compilation are
  verified; no ESP32 HTTP session was run for this candidate.
- `sh host-tests/run.sh`: **PASS**, including frontend JavaScript executed from
  the embedded page against a stateful HTTP-contract adapter (empty/open,
  assemble, save/revision, reload, delete, errors, cursor dedupe/loss/reboot),
  shared route matcher, log ring and cursor tests, and LED mapping vectors.
  The frontend adapter is not the Arduino `WebServer` runtime; only the actual
  firmware handlers are compiled by the integration build.
- `sh firmware/bytereel_2026/build.sh`: **PASS**, Arduino ESP32 core 2.0.14,
  attached 3 MiB app / 896 KiB LittleFS partition; sketch 960,145 B, static
  globals 107,232 B. Application image SHA-256:
  `a97d2ce4566e134b083ade7dcc2efc63069ec5f73b4fc9fb1c5d4651184ce17b`.
  This image was not uploaded. Physical LED orientation, actual browser-to-board
  HTTP behavior, and the exact expander part remain **NOT TESTED**.

Status labels distinguish automated evidence, maintainer-reported physical
observations, and tests that still need to be run.

## Historical automated checks and builds before r3

The following entries document earlier builds and device sessions. They do not
describe the deployment status of the r3 image above.

### PASS — automated checks and build

- `sh host-tests/run.sh`: VM smoke coverage for all 59 dispatch cases; focused
  tests for ALU/Z, COUNT/LOOP, invalid arguments and banks, PC boundary,
  WAIT/AWAIT/VWAIT, STEP, UART RX byte `0x00`, timeouts, STOP cancellation,
  piezo state, RNG, and SEED.
- RunGate boot STOP-to-RUN interlock, HALT/reset/fault re-arming, panel debounce,
  long-gesture cancellation, video renderer, fetch FIFO, log ring, IPS sampler,
  and slot commit algorithm fault-adapter coverage.
- I2C regression host checks: source guard requires direct `requestFrom(0x22, 2)`
  with no write-address probe; diagnostic-state tests cover short reads,
  separate read/write counters, retention of the last-good sample, and last
  error metadata. Panel-state tests cover invalidation, 30 ms reacquisition,
  suppressed recovery edges, and restarting long-hold timing. These tests do
  not exercise a physical I2C bus.
- Host assembler matches all six supplied 256-byte golden examples.
- `python3 tools/validate_package.py`: 59 unique opcode values, all 45 legacy
  values retained, example images and slot source/bytecode fixtures validated,
  and partition layout checked.
- Main regression-fix build: Arduino ESP32 core 2.0.14, 3 MiB app + 896 KiB
  LittleFS partition. **PASS**: 948,096 B flash, 107,200 B static RAM.
  Application SHA-256:
  `6ad947619aa583f43be301a1f0a18588f1a3bd17d5c5ae8c94dc2edfde8c78de`.
  Partition image SHA-256 (unchanged):
  `aaae2888c5a6a348004b5b436f47abb25ae32e72d9003902955a998eda723edd`.
- At the time of this historical report, that image had not been uploaded; it
  was superseded by later r2/r3 candidates.
- The previous main firmware upload on 2026-11-19 passed esptool image-hash
  verification. Port `/dev/cu.usbserial-0001`; chip ESP32-D0WD-V3 revision 3.1,
  4 MiB flash. Core 2.0.14 upload recipe used bootloader at `0x1000`, partition
  at `0x8000`, `boot_app0` at `0xE000`, and application at `0x10000`; no
  `erase_flash` command was used.
- Main firmware upload on 2026-11-19: **PASS**. Port `/dev/cu.usbserial-0001`;
  esptool identified ESP32-D0WD-V3 revision 3.1 and 4 MiB flash. The Arduino
  core 2.0.14 upload recipe wrote bootloader at `0x1000`, partition table at
  `0x8000`, `boot_app0` at `0xE000`, and application at `0x10000`. esptool
  reported verified hashes for each image. No `erase_flash` command was used.
- Before the historical 2026-11-19 upload, a private local capture was
  checked and the old SPIFFS region contained only `0xFF` bytes. Raw flash
  captures are intentionally excluded from this public source release. No
  filesystem format was performed.
- The separate LittleFS test sketch compiled, but it was not uploaded or run.

## Historical physical observation before r3

The maintainer reports that Wi-Fi and the web monitor appear to work. This
observation was not independently reproduced by the host: the host was not
connected to the device AP and did not query its HTTP API.

## NOT TESTED or not confirmed

- Source-level I2C regression found: the previous read path sent an
  `address + WRITE` transaction to input expander `0x22` using
  `beginTransmission()` / `endTransmission(false)` before requesting a read.
  The verified service firmware reads this device directly with
  `Wire.requestFrom(0x22, 2)`, and the device is specified read-only. The
  application aborted the input read whenever that extra transaction returned
  nonzero. The candidate removes the write-address phase. The older status
  exposed only an aggregate counter, so it did not preserve the return code;
  the exact target-side NACK/result and the fix's physical effect still need
  confirmation after a controlled flash. (Historical status before r3.)
- The earlier application serial capture at 115200 baud contained an unreadable
  repeating binary stream; no boot text or reset-cause line could be recovered.
  The r3 application was uploaded on 2026-11-20, but its serial output has not
  been captured. Reset cause, expander state, current LittleFS mount result,
  and boot heap values remain **NOT CONFIRMED** for r3.
- A prior device boot reported `STORAGE_READY`; r3 has not had a post-upload
  storage status capture. The firmware mounts with `formatOnFail=false`; after
  a mount failure it checks the entire `spiffs` partition directly on the
  ESP32. It initializes only a
  fully erased partition (`0xFF` throughout) while the physical switch is
  STOP. Non-erased data and read failures remain untouched and are reported
  separately.
- HTTP handler integration tests do not invoke the actual Arduino WebServer
  callbacks. Two-client revision races, panel-save conflicts, STOP_REQUIRED,
  BUSY, malformed requests, body limits, and RAM isolation have not been
  tested end-to-end.
- No actual LittleFS ENOSPC or physical power-cut test. Host fault injection
  covers the shared commit algorithm through a simulated fault adapter; the
  on-device harness only compiles.
- VM tests do not exhaust every opcode argument and every mode/timing/Z
  combination.
- No measured runtime free/minimum heap or largest DMA block, no sustained
  Wi-Fi + PAL + storage load, no 100-switch AV test, no audio underrun test,
  and no verified PAL, DAC audio, piezo, UART wiring, LED, panel, or power
  measurements. Requested IPS is not a measured IPS result.
- USB binary-stream symptom is not attributed to a confirmed cause. Source
  audit finds `Serial.begin(115200)` and printable `Serial.printf/println`
  diagnostics on UART0; binary bytes are sent with `Serial2.write()` only,
  while UART2 is explicitly mapped to RX32/TX27 at 9600 baud. No
  `Serial.write`, `Serial.updateBaudRate`, or Serial use in the composite-video
  library was found. The new boot banner will help classify a subsequent
  readable capture.

## Physical acceptance checklist

Use [`UZIVATELSKY_NAVOD.md`](UZIVATELSKY_NAVOD.md). Test in this order: boot
and panel; Wi-Fi and slots; piezo and UART; audio; PAL; concurrent load and
repeated mode switching. Record serial output, reset cause, expander result, LittleFS state,
SSID/IP, `freeHeap`, `minHeap`, `largestDmaBlock`, requested/measured IPS, and
all AV/UART counters. A one-time initialization is allowed only after the
firmware verifies the whole partition is erased. Do not overwrite existing
user data.

## I2C regression check (no RUN required)

1. The r3 application is already installed. Keep the physical RUN switch at
   STOP and VIDEO/SOUND at AUDIO. Do not change partition or format LittleFS.
2. Open USB Serial at 115200 baud. Capture the boot and confirm the text banner
   reports I2C SDA 21, SCL 22, 10000 Hz, USB UART0 115200, and peripheral
   UART2 RX32/TX27 at 9600 baud. Confirm boot does not enter RUN.
3. Open the read-only status page/API and note `i2c.inputReadErrors` and
   `i2c.ledWriteErrors`. For 30 seconds, toggle several data switches, HOME,
   NEXT, and the speed potentiometer while leaving RUN at STOP and mode at
   AUDIO.
4. Confirm the LED display follows the panel, `lastRead.requested`,
   `lastRead.returned`, and `lastRead.received` are all 2, `lastValidInput.raw`
   changes with the switches, its age stays small, and decoded active controls
   match the panel. Both I2C error counters should remain unchanged. If either
   rises, record `lastError`, the raw input, and the serial text; do not reset
   the counters or change wiring.

## Historical panel, slot, and storage update (2026-11-19)

- Host suite: **PASS** (`sh host-tests/run.sh`). Added coverage for LED row
  mapping (`01`, `80`, `55`, `AA`), one-hot slot selection, EDIT preview and
  EXAM behavior, held-selection cancellation, error pulse restoration, and
  format recovery decisions. Slot algorithm tests inject deletion failures
  and verify a valid candidate remains readable.
- ESP32 build: **PASS**, Arduino ESP32 core 2.0.14; compiler reports
  952,013 B of 3,145,728 B application capacity and 107,216 B static globals.
  The `.ino.bin` artifact is 958,304 B. Partition table and pin map are
  unchanged.
- Firmware image: `firmware/build/bytereel_2026.ino.bin`, SHA-256
  `adfb1bbf5315681aba07a0feb9eb5e5e01ed1a1ef15559f65854b32d8ec72953`.
- Physical LittleFS scan/initialization, panel LED orientation, SAVE/LOAD, web
  delete, and AV behavior remain **NOT TESTED** on the device. Host tests cover
  shared state and storage algorithms, not ESP flash I/O or physical LEDs.
