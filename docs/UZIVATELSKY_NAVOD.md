# byteREEL 2026 — user guide and hardware acceptance

## Build and upload

1. Back up programs on the device before changing its partition table. The
   current partition moves the LittleFS region; the firmware does not migrate
   or autoformat the old filesystem.
2. Install Arduino ESP32 core **2.0.14**, select **ESP32 Dev Module**, classic
   ESP32, 4 MiB flash, 240 MHz, and no PSRAM assumption. Use the exact library
   versions in `firmware/bytereel_2026/dependencies.lock`.
3. From the repository root, run `sh firmware/bytereel_2026/build.sh`. The
   outputs are in `firmware/build/`. The build does not upload.
4. When uploading a new partition layout, use the core's complete upload
   recipe, including bootloader, partition table, `boot_app0`, and application
   image. Do not use **Erase All Flash**. Never format a used filesystem
   without a verified backup and explicit authorization.

At boot, firmware first mounts the existing `spiffs` partition with
`formatOnFail=false`. If mount fails while the physical switch is in STOP, the
firmware reads the entire partition directly on the ESP32. It formats once
only when every byte is `0xFF`, then mounts and initializes the slots folder.
If any byte contains data, or the full scan cannot be verified, it leaves the
partition untouched and reports the reason in the status page and serial log.
There is no general format-on-failure behavior.

## First-start behavior

The firmware runs an LED initialization and a short piezo sequence at boot.
The VM must remain stopped even if the RUN switch is already in RUN at boot;
use a physical STOP-to-RUN transition to start execution. STOP must also
interrupt timed waits and UART waits, and mute the audio and piezo outputs.

The open access point is named `byteREEL-2026-XXXX`, where the suffix comes
from the unit MAC address. The configured device address is
`http://192.168.4.1`. The access point and web server run only in MEMORY. Boot
in EDIT leaves Wi-Fi off; changing MEMORY to EDIT disconnects web clients and
stops the AP. The web interface edits saved slots and provides read-only
status/log monitoring; it has no device RUN/STOP/LOAD controls and does not
edit the live work banks.

## Panel gestures

In STOP + EDIT, the upper row is the current address. Moving a data switch
shows a temporary input preview on the lower row and never writes RAM. WRITE
stores the switch value; WRITE+ stores and advances. EXAM exits an active
preview without changing the address. With no preview active, EXAM selects the
address from the switches. Release EXAM between actions. HOME selects address
0 and NEXT advances to the next address.

In MEMORY, use exactly one data switch to select a slot: D0 selects slot 1,
through D7 for slot 8. The upper LEDs show the selection; the lower LEDs blink
only for slots with a valid saved record. Hold MEMORY + EXP/EXAM for two
seconds to save both working banks. Hold MEMORY + NEXT for two seconds to
load both banks. Slot changes during a hold cancel that gesture; release the
operation paddle after it completes. A successful load resets PC and editor
address to 00 and leaves the VM stopped. Invalid selection or an
unavailable/corrupt filesystem is reported without changing RAM or the saved
slot.

In MEMORY + STOP, a short WRITE loads the built-in demo for the active AUDIO
or VIDEO mode into that mode's working bank only. It preserves the other bank
and saved slots, resets VM state and outputs, and does not start execution.
WRITE+ does not load a demo. The AUDIO demo plays a repeating three-level
melody through the audio output; the VIDEO demo cycles through three patterns.

Panel expander stays on the verified 10 kHz I2C bus. In EDIT preview, active
DATA bits blink together at 1000 ms on / 1000 ms off while the address remains
steady. In MEMORY, the selected slot mask is shown above and the valid-slot
occupancy mask blinks below at 1000 ms on / 1000 ms off. Normal EDIT display
stays steady. User operation errors
flash all LEDs three times at 100 ms on / 100 ms off, then restore the panel
display. These patterns are scheduled without blocking delays and redundant
I2C writes are skipped.

## Physical acceptance sequence

Run these checks only with suitable AV equipment and a safe power setup. Keep
the input expander at `0x22` read-only. Do not connect RS-232 or 5 V to the
3.3 V UART pins.

1. **Boot and panel:** Connect a 115200 baud serial monitor. Confirm the LED
   sequence. Boot once with RUN set to RUN and confirm the VM remains stopped.
   Test STOP-to-RUN, then confirm HALT, VM error, and RESET each require a new
   STOP-to-RUN transition. Test EDIT preview, EXAM, HOME/NEXT, WRITE/WRITE+,
   MEMORY one-hot slot selection, SAVE, LOAD, and CLR only with disposable
   data. Confirm that paddle preview does not write RAM and that EXAM actions
   require separate presses.
2. **Wi-Fi and slots:** Connect to the open AP and open `http://192.168.4.1`.
   Check the slot list, source editor, assembler preview, import/export, and
   revision-conflict behavior and slot deletion confirmation. Verify that a
   web save does not alter the live working RAM. A completely erased
   filesystem partition initializes at boot in STOP. If it contains data or
   cannot be fully checked, the firmware leaves it untouched and reports the
   mount problem.
3. **Piezo and UART:** Test the piezo in AUDIO and VIDEO and confirm STOP
   silences it. GX12 is pin 1 = TX/GPIO27, pin 2 = RX/GPIO32, pin 3 = GND.
   With power off, verify pin 3 continuity. Then use a pin 1–2 loopback and
   test bytes `00` and `FF` at 9600 baud.
4. **Audio:** Connect GPIO26 audio RCA to a line input at low volume. Test
   frequency, waveform, volume, gate, filter, and STOP mute.
   For a steady-tone diagnostic, assemble `examples/audio-440-diagnostic.asm`
   into an AUDIO slot in MEMORY, hold MEMORY + NEXT for two seconds while
   stopped, then switch to EDIT + AUDIO and perform a physical STOP-to-RUN.
   It loops a 440 Hz sine at VOLUME 40, 80 and C0 for 2.5 seconds each without
   gate retriggers. Record continuous crackle separately from the clicks at
   level changes. In MEMORY, the VM is stopped by design, so the same tone
   cannot run while Wi-Fi is active. Check the cumulative audio write counters
   in status after each phase; `audioDmaUnderruns` is deliberately null because
   the firmware does not read a hardware DMA-underrun event.
5. **PAL:** Connect GPIO25 composite video to a PAL-capable input. Check
   synchronization, color, frame margins, FEEDBACK, hold on STOP, and mode
   changes without an MCU restart.
6. **Concurrency and switching:** During PAL, load the web monitor and slot
   pages from two clients. Check STOP latency, free/minimum heap, largest DMA
   block, dropped frames, and audio underruns. Save only while physically
   stopped. Perform at least 100 AUDIO/VIDEO switches and check for resets,
   memory loss, or growing resource usage.

Record the actual serial output, reset cause, expander status, LittleFS state,
SSID/IP, `freeHeap`, `minHeap`, `largestDmaBlock`, and requested and measured
IPS separately. An ACK, successful build, or requested IPS setting does not
prove a physical signal, measured execution rate, or continuity of GND.

## Canonical wiring

The maintained pin map is in [`../spec/HARDWARE.md`](../spec/HARDWARE.md).
Do not follow the superseded GX12 pinout in historical materials under
`reference/`.
