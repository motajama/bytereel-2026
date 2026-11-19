# Firmware build

Target: ESP32-WROOM-32 / classic ESP32, 4 MiB flash, Arduino ESP32 core
2.0.14. The pinned dependencies and local composite-video fork are described
in `dependencies.lock` and `firmware/libraries/`.

From the repository root:

```sh
sh firmware/bytereel_2026/install-deps.sh
sh firmware/bytereel_2026/build.sh
```

The script uses the checked-in Huge APP partition layout (3 MiB application,
896 KiB LittleFS) and writes build outputs to `firmware/build/`. It does not
upload, erase, or format a connected device. Use the core 2.0.14 upload recipe
or the Arduino CLI upload workflow with all generated bootloader, partition,
`boot_app0`, and application images; do not upload the application image alone
when changing partition layouts.

## Firmware modules

- ISA table, VM, and assembler
- Panel filter, run interlock, LED monitor, piezo, and UART integration
- Audio DAC and PAL composite-video backends
- Paired slot persistence, local HTTP API, static web UI, and event/status logs

Candidate `2026.11.19-r3` was uploaded as an application-only image at
`0x10000` on 2026-11-20; esptool verified the write hash. The partition table
and LittleFS were not written. No post-upload boot capture or functional test
was performed. Consult the repository root `docs/TEST_REPORT.md` for its
digest, automated results, and outstanding hardware checks.

The boot storage path intentionally mounts without format-on-failure. A prior
device boot reported `STORAGE_READY`; the r3 application has not had a new
post-upload storage capture. A failed mount never triggers general automatic
formatting. The explicitly guarded first-start path only formats a partition
the firmware verifies as entirely erased while the physical switch is STOP.

See [`../../docs/UZIVATELSKY_NAVOD.md`](../../docs/UZIVATELSKY_NAVOD.md) for
operation and physical acceptance checks.
