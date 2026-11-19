# byteREEL 2026

byteREEL is an open hardware and software 8-bit creative computer built around
an ESP32-WROOM-32. It combines a two-byte virtual machine, a physical switch
panel, composite PAL video, mono DAC audio, a piezo output, UART, and local
Wi-Fi slot editor.

This repository contains the firmware, VM and assembler, host tests, reference
programs, hardware interface documentation, and a prepared directory layout
for the forthcoming CAD and drawing files. The current hardware notes describe
the assembled prototype; manufacturing-ready schematics, PCB CAD, and
mechanical drawings have not yet been added. See [`hardware/README.md`](hardware/README.md).

## Project status

- Firmware: candidate `2026.11.19-r3` was built with Arduino ESP32 core
  2.0.14 and flashed as an application-only update at `0x10000` on 2026-11-20.
  Esptool verified the written image hash. The existing partition table and
  filesystem were not written. See [`docs/TEST_REPORT.md`](docs/TEST_REPORT.md)
  for the exact digest and automated test results.
- Hardware: the upload identified an ESP32-D0WD-V3 revision 3.1 with 4 MiB
  flash. No serial boot capture or post-upload functional test was performed
  for r3. Wi-Fi/HTTP, panel, storage, PAL, audio, piezo, UART, and sustained
  load remain subject to the documented physical acceptance checks.
- Storage: LittleFS was left in place during the application upload. The
  firmware does not autoformat after a mount failure; blank-partition
  initialization is limited to its explicit erased-partition check.

Build and host-test status is not a substitute for physical verification.

## Quick start

Requirements: Arduino CLI, Python 3, and an ESP32 Dev Module (classic ESP32,
4 MiB flash, no PSRAM assumed). Exact firmware dependencies and versions are
listed in [`firmware/bytereel_2026/dependencies.lock`](firmware/bytereel_2026/dependencies.lock).

```sh
sh firmware/bytereel_2026/install-deps.sh
sh host-tests/run.sh
python3 tools/validate_package.py
sh firmware/bytereel_2026/build.sh
```

The build creates the main application, bootloader, and partition image under
`firmware/build/`. It does not upload or erase the device. The selected
partition layout provides a 3 MiB application and 896 KiB LittleFS region.
Back up any existing device data before changing partition offsets.

The checked-in `.bin` files are build outputs for the candidate described in
the test report. Rebuild with the pinned toolchain before making a new release.

## Documentation

- [Firmware and ISA specification](spec/FIRMWARE-SPEC.md)
- [Hardware interface and canonical pinout](spec/HARDWARE.md)
- [Instruction set reference](spec/INSTRUCTIONS.md)
- [Storage and local web API](spec/STORAGE-WEB.md)
- [User guide and physical acceptance steps](docs/UZIVATELSKY_NAVOD.md)
- [Automated and hardware test report](docs/TEST_REPORT.md)
- [Release notes](docs/RELEASE_NOTES.md)

## Licensing

- Original software, firmware, assembler, tests, and sample programs: MIT; see
  [`LICENSE`](LICENSE).
- Hardware design files and hardware documentation: CERN-OHL-S-2.0; see
  [`LICENSES/CERN-OHL-S-2.0.txt`](LICENSES/CERN-OHL-S-2.0.txt) and the scope
  notes in [`LICENSES/README.md`](LICENSES/README.md).
- Documentation: CC BY 4.0, unless a file says otherwise; see
  [`LICENSES/README.md`](LICENSES/README.md).
- Bundled composite-video library files retain their upstream MIT notices.
  Third-party and historical reference files are identified separately and
  are not relicensed by the project-wide license declarations.

## Contributions

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for source layout, build checks,
hardware change records, and drawing conventions. Please do not submit a pin
remap without updating the hardware specification and the firmware together.
