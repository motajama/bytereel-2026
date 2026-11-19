# Hardware design files

This directory is the source-of-truth location for byteREEL hardware design
materials. The existing unit was assembled and its pinout was documented, but
this repository does not yet contain a verified schematic, PCB layout, panel
CAD, enclosure model, or dimensioned manufacturing drawing. Those files must
be created from the physical device and checked before fabrication; do not
interpret the folder structure as evidence that a CAD design exists.

## Directory layout

| Directory | Intended contents |
|---|---|
| `cad/electrical/` | Editable schematic source and project-level electrical notes |
| `cad/pcb/` | Editable PCB source, footprints, and board constraints |
| `cad/mechanical/` | Editable panel/enclosure source models |
| `drawings/electrical/` | Reviewed wiring diagrams and schematic exports (PDF/SVG) |
| `drawings/mechanical/` | Dimensioned panel and enclosure drawings (PDF/DXF/SVG) |
| `drawings/assembly/` | Assembly views, connector orientation, and cable routing |
| `exports/` | Fabrication outputs generated from the editable sources |

## Current electrical interface

The established pinout and assembly notes are in [`../spec/HARDWARE.md`](../spec/HARDWARE.md).
In particular, the input expander at I2C address `0x22` is read-only and the
GX12 connector is pin 1 = TX/GPIO27, pin 2 = RX/GPIO32, pin 3 = GND. Do not
change these assignments based on superseded reference documents.

## Drawing release checklist

Before a drawing is considered fabrication-ready, include units, scale,
revision/date, connector mating/view direction, pin numbering, component
values and ratings, board outline and mounting dimensions, and a note for each
dimension that was not measured. Keep editable CAD sources in the repository;
PDF/DXF exports alone are not the complete hardware source.

Hardware materials are licensed under CERN-OHL-S-2.0. See
[`../LICENSES/README.md`](../LICENSES/README.md).
