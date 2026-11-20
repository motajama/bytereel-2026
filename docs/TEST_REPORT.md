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

## AV, slot upload, and built-in demos candidate (2026-11-20)

- Host suite: **PASS** (`sh host-tests/run.sh`). Added real helper coverage for
  top-level JSON field scoping against escaped assembler text, nested bank
  objects and UTF-16 escapes; demo array/binary equality and execution through
  the ISA VM; audio DAC word range/alignment and block-boundary continuity;
  slot occupancy, demo-bank isolation and the existing storage/revision fault
  tests. The browser test uses the checked-in frontend and shared route/JSON
  helper contracts, but does not instantiate Arduino `WebServer` callbacks.
- ESP32 build: **PASS**, Arduino ESP32 core 2.0.14. Generated partition table
  was decoded and matches the checked-in layout: app0 `0x10000/0x300000`,
  LittleFS `0x310000/0x0E0000`, coredump `0x3F0000/0x10000`. Compiler reports
  972,373 B application and 107,312 B global variables. This link-time figure
  is not proof of runtime heap headroom.
- Image: `firmware/build/bytereel_2026.ino.bin`, 978,672 B,
  SHA-256 `3d0a8c6b3b52bc7e2309bacab69954ec948a31a616cadb39b0a42d3bb9719f34`.
- Video source diagnosis: the app previously reduced every `begin()` failure
  to `VIDEO_BACKEND_ERROR`; the composite library exposed neither a stage nor
  its `esp_err_t`. It now reports PAL/framebuffer/DMA/interrupt/I2S phase and
  result, plus heap and DMA-block values; failed setup frees partial buffers
  and interrupt resources. The old slot commit path also stopped and restarted
  PAL around LittleFS and ignored `begin()` failure. That unnecessary pause is
  removed. The exact reason for the user's observed first `PATTERN` failure is
  not provable from the supplied log; a device capture after this image is
  needed to identify its stage/code.
- Slot upload parsing now resolves top-level fields and nested slot/bank
  objects instead of searching the first matching key anywhere in the body.
  Thus quoted assembler text containing words like `slot` or `video` cannot
  redirect validation. Field-specific format/revision errors remain enforced.
- Audio host checks confirm the renderer emits centered, in-range, correctly
  aligned DAC samples and does not discontinuously reset its sine phase at a
  128-sample boundary. The audio task now completes partial I2S writes within a
  bounded block deadline and exposes maximum write duration/underrun counters.
  This does not establish whether the reported physical crackle is fixed.
- MEMORY lower-row LED mask now comes from validated slot records, including
  valid zero-byte-content records; corrupt/read-error slots remain in a
  separate mask and are not silently classified as empty. Demo binaries are
  generated from the checked-in assembler sources and checked against the
  firmware arrays on every build. The audio demo cycles VOLUME 40/80/C0; the
  video demo cycles patterns 3/4/6. Short WRITE in MEMORY+STOP replaces only
  the active work bank; it does not save, load, or run a user slot/program.
- Physical PAL output, repeated AUDIO/VIDEO switching, LittleFS save/load,
  Wi-Fi stability under load, LED occupancy, and acoustic crackle are **NOT
  TESTED** on the device. No firmware upload was performed for this candidate.

## Slot 2 source mismatch and audio transport candidate (2026-11-20)

- Source-level slot cause: opening a slot launched an asynchronous GET whose
  response unconditionally wrote into global editor fields. An older response
  could replace a newer selection. SAVE also held slot/revision/source partly
  in globals while asynchronous assembly updated the live HEX fields; changing
  selection during assembly could pair one slot's source with another editor's
  bytes. The checked-in frontend test injects both delayed GET and delayed
  assembly responses and verifies they cannot overwrite or save against the
  newly selected editor. SAVE now snapshots slot, revision, both banks and
  metadata, checks an editor generation token after each await, and sends a
  slot-specific immutable body.
- Assembler input edits are assembled before SAVE and the returned 256-byte
  bank is the exact bank in the payload. Direct HEX edits retain the user's
  source text with `sourceMatches:false` and unknown length rather than silently
  dropping it or claiming it matches. Panel-generated banks preserve existing
  source strings as stale metadata too. Null source remains supported for
  source-less banks. The server still validates current sources strictly and
  now reports slot, bank, first differing address, expected byte and supplied
  byte. A host test passes the frontend's real request body through the C++ ISA
  assembler and the same source/bytecode comparison helper used by the handler.
- The integration sequence covers saved slot 1 followed by empty slot 2,
  first and repeated slot 2 saves, reopening, exact padded HEX/source match,
  panel bank publication, HEX change, stale source retention, and all route
  indices 0–7. The test uses the actual frontend JavaScript, C++ assembler,
  production JSON scanner/source checker, and bank publication helper. It is
  an HTTP adapter test, not an invocation of Arduino `WebServer` callbacks or
  LittleFS on the ESP32.
- Panel `SLOT_SELECT` logs now label zero mask as `NONE`; selection changes
  alone do not flash the error animation. Invalid masks are still rejected
  and signaled only when panel SAVE/LOAD is attempted. Web slot routes remain
  independent of the panel selector.
- Audio source review found that a partially written block was abandoned when
  an I2S call returned an error/timeout, while the synthesizer had already
  advanced its phase by 128 samples. The audio task now keeps a write cursor and
  retries only the unsent suffix, so it does not silently skip that remainder.
  Oscillator/envelope/filter controls are snapshotted once per block so VM
  parameter changes cannot alter a block halfway through. Low-cost status
  counters report actual bytes, partial I2S calls, API timeouts, software
  deadline expirations, incomplete block attempts, completed blocks, maximum
  write/fill durations and block periods. `audioDmaUnderruns` is `null` because
  this code does not subscribe to a hardware DMA-underrun event; it is not
  inferred from failed writes.
- Audio host tests inject partial writes and timeout/retry sequences through
  the production cursor/stats helpers, and execute
  `examples/audio-440-diagnostic.asm` through the ISA VM to confirm fixed 440 Hz
  sine, fixed gate/phase, 40/80/C0 levels and repetition. This does not measure
  actual I2S/DAC timing or audible crackle.
- Wi-Fi runs only in MEMORY while VM RUN is accepted only in EDIT. Therefore
  the current control model cannot play the diagnostic tone concurrently with
  Wi-Fi. Status counters can be compared for audio silence/write scheduling
  under MEMORY/Wi-Fi load versus tone blocks in EDIT, but that is not an
  equivalent acoustic A/B. A simultaneous tone-under-Wi-Fi test would require
  a separately authorized diagnostic playback path or a change to that mode
  boundary; neither is hidden in this candidate.
- ESP32 build: **PASS**, core 2.0.14 and the same partition table. Runtime
  audio transport, slot 2 persistence and panel LOAD are still **NOT TESTED**
  on hardware. No upload, flash read, format or slot deletion was performed.
- Final build report: **979,121 B** application text/data, **107,352 B** global
  variables; generated Huge APP partition was decoded and matches the existing
  layout (`app0 0x10000/0x300000`, `spiffs 0x310000/0x0E0000`, coredump
  `0x3F0000/0x10000`). Artifact `firmware/build/bytereel_2026.ino.bin` is
  **985,424 B**, SHA-256
  `292a31025f525c64010bb7001c71531b061a5109b112665775f8ec76b1549410`.
- Generated diagnostic image `examples/audio-440-diagnostic.bin` is 256 B,
  SHA-256 `765e9498ef5785220765951d7b1860c2e48c46f6524a23517fc6c2dcad023268`.

## Web request tracing and audio path diagnostic (2026-11-20)

- Confirmed frontend defects: opening an editor also launched a redundant,
  unawaited slot-list fetch alongside the slot GET; the editor UI embedded
  `ui-2026.11.20-r1` while firmware status advertised `r2`; and a manual HEX
  edit did not clear `sourceEdited`, so a subsequent SAVE could reassemble old
  source and replace the user's HEX edit. The UI now uses `r2`, allows at most
  one slot-list request, does not refresh the list on editor open, blocks slot
  switching while a SAVE is in flight, and makes direct HEX edits explicitly
  source-stale while preserving the source text.
- SAVE uses a longer operation-specific deadline and never blindly retries.
  If the response times out, the page GETs the selected slot and compares both
  complete banks, sources, lengths and revision-bearing record before it says
  the commit succeeded. A mismatch or failed verification is reported as
  unknown/not committed; the user can reload manually. Status and log polling
  pause during SAVE to keep the synchronous `WebServer` queue clear. Each
  request carries a UI client ID; handled requests return
  `X-byteREEL-Request-Id` and the same ID in error JSON. Firmware records method,
  path, Content-Length, phase, handler duration and HTTP status for mutations,
  assembler/list requests, errors and slow polls. Routine fast status/log polls
  are omitted from the ring to avoid displacing user events.
- The WebServer callback starts only after its HTTP parser has received and
  parsed the request body. A timeout before callback entry therefore cannot
  have a firmware request ID or phase log; its UI client ID is still shown. The
  existing device did not provide the timed-out request's client ID/log, so the
  exact physical timeout point remains **NOT PROVEN**. A new device trace can
  distinguish a handled slow operation from a request that never reached a
  callback, but cannot identify a pre-callback Wi-Fi/TCP cause from firmware
  logs alone.
- The frontend regression now runs the exact production JavaScript request
  body through the production JSON scanner and source/bytecode validation
  helper now called by the real `apiPutSlot` handler. The slot 2 reproduction
  `PFREQ 18 / PDUR 00 / PPLAY 00 / JMP 06` assembles to
  `80 18 81 00 82 00 30 06`, with a 256-byte HEX bank, sourceMatches=true,
  route index 1, expectedRevision 0 and a source-less empty VIDEO bank. The
  test validates the first empty save, reopen/readback, panel bank publication,
  direct HEX change with stale-source retention, syntax rejection with line,
  mismatch address/expected/supplied bytes, slot indices 1–8, and SAVE race
  protection. This is a host HTTP adapter: it uses the actual frontend and
  production body/record parser and validator, but does not call Arduino's
  `WebServer` callback or persist to ESP32 LittleFS. The stated PFREQ sequence
  passes this path; the historical device-side refusal has no captured request
  body/requestId, so its exact original trigger is not proven.
- Audio source/configuration calculation: I2S is configured for 22,050 Hz,
  16-bit, `ONLY_RIGHT`, `dma_buf_len=128`, `dma_buf_count=8`. Under the
  configured mono interpretation, one 128-sample producer block represents
  5.805 ms and eight descriptors represent about 46.44 ms (2,048 bytes) of
  queue capacity. Producer writes have a 10 ms software deadline and preserve
  partial-write cursors. Current status reports actual API bytes, partial
  calls, timeouts, write duration, block fill/period and missed software
  deadlines; `audioDmaUnderruns` remains `null` because hardware DMA underruns
  are not observed by this driver. The device has not supplied these counters,
  so no producer-vs-DMA timing result or cause of the audible crackle is
  claimed.
- Added a build-flag-only direct-output diagnostic: a precomputed 2,205-sample
  period contains exactly 44 cycles at 440 Hz/22,050 Hz, is emitted through the
  same I2S built-in DAC path at a fixed mild amplitude, and does not call VM,
  synth, envelope or filter code. RUN after the normal STOP interlock enables
  it; physical STOP disables the DAC after the current bounded I2S write
  returns (at most its 10 ms API timeout) and clears the tone gate. It is
  intended to compare EDIT (Wi-Fi off) and MEMORY (Wi-Fi on), then compare the
  diagnostic sine to the production synth. Host checks prove table range and
  periodic boundary continuity only, not real DAC timing or audible quality.
- `sh host-tests/run.sh`: **PASS**, including the frontend → assembler → actual
  production request/record validation helper path and focused audio table and
  partial-write tests. This does not represent a real HTTP/TCP/LittleFS test.
- Standard firmware build: **PASS**, ESP32 core 2.0.14; 985,453 B application,
  107,528 B static globals, 3,145,728 B app partition limit. Image
  `firmware/build/bytereel_2026.ino.bin`, SHA-256
  `9d6a4ed276e8528c21b71616246dbd396e67366f900b10566631eb4df4683c51`.
- Diagnostic build: **PASS**, same core and partition table; 983,733 B
  application, 111,944 B static globals. Image
  `firmware/build/audio-diagnostic/bytereel_2026.ino.bin`, SHA-256
  `19c833e660cbb4f0f58a9e9695dcfe2cd4dd07d45cb715811a607d27aa3d881d`.
  Both generated partition tables compare byte-for-byte and decode to the
  existing app0 `0x10000/0x300000`, SPIFFS `0x310000/0x0E0000`, and coredump
  `0x3F0000/0x10000` layout. No firmware was uploaded and no user data was
  accessed.
- The patch does not alter video backend code, panel SAVE/LOAD logic, speed,
  I2C setup, ISA, LED mapping, partition, or Wi-Fi mode policy. Video and panel
  SAVE/LOAD were physically working in the user's last report; this software
  candidate preserves those paths, but no new physical regression test was
  performed. Wi-Fi stability, device HTTP latency, LittleFS web commit, and
  acoustic crackle remain **NOT TESTED** on hardware.
- Targeted physical test after controlled upload: in AUDIO, leave device STOP
  for silence; switch STOP→RUN to hear the fixed diagnostic sine, then STOP to
  confirm immediate DAC mute. Compare short periods in EDIT (AP off) and
  MEMORY (AP on) while watching `audioWriteCalls`, `audioPartialWrites`,
  `audioWriteTimeouts`, `audioBlockDeadlineExpirations`, `audioMaxWriteUs`,
  `audioMaxBlockPeriodUs`, free heap and largest DMA block. Return to the
  standard firmware before testing normal VM audio; then test the same steady
  440 Hz synth program. For web, save slot 2 and note UI version, client/server
  request IDs and the single `WEB_REQUEST` phase/status line; do not repeat a
  timed-out SAVE until its GET verification has completed.
