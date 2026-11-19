# Contributing

Thanks for helping improve byteREEL 2026. Please keep firmware behavior,
hardware documentation, and tests in sync.

## Before opening a change

Run the relevant checks from the repository root:

```sh
sh host-tests/run.sh
python3 tools/validate_package.py
sh firmware/bytereel_2026/build.sh
```

Do not claim a physical test based on a host test or a successful firmware
build. Label device results with the board, core version, wiring, observed
output, and test date. Never use the production filesystem as a destructive
fault-injection target.

## Hardware changes

The pin map in `spec/HARDWARE.md` is the canonical interface. A hardware change
must update the relevant firmware configuration, interface documentation,
assembly notes, and drawings together. Do not infer the connector orientation
from an old manual; the current GX12 assignment is pin 1 TX27, pin 2 RX32,
pin 3 GND.

## CAD and drawing files

Place editable electrical sources under `hardware/cad/electrical/`, PCB sources
under `hardware/cad/pcb/`, and mechanical CAD under `hardware/cad/mechanical/`.
Place review/export drawings (PDF, SVG, DXF) under `hardware/drawings/` and
manufacturing exports under `hardware/exports/`. Keep native editable sources
alongside exports. Record the tool and version used, units, scale, revision,
connector view, and any dimensions that are not verified on the physical unit.

## Licensing

Contributions are submitted under the license assigned to the changed files:
MIT for original software, CERN-OHL-S-2.0 for hardware design source, and CC
BY 4.0 for project documentation. Preserve all third-party notices. See
`LICENSES/README.md`.
