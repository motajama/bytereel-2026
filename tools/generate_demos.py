#!/usr/bin/env python3
"""Generate/verify the immutable firmware demo arrays from their ISA source."""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "examples"
HEADER = ROOT / "firmware/bytereel_2026/demo_programs.h"

spec = importlib.util.spec_from_file_location(
    "bytereel_assembler", ROOT / "firmware/bytereel_2026/assembler.py"
)
assembler = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(assembler)


def render_header(programs: dict[str, bytes]) -> str:
    lines = [
        "#pragma once",
        "#if defined(ARDUINO)",
        "#include <Arduino.h>",
        "#include <pgmspace.h>",
        "#else",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "#ifndef PROGMEM",
        "#define PROGMEM",
        "#endif",
        "#ifndef pgm_read_byte",
        "#define pgm_read_byte(p) (*(p))",
        "#endif",
        "#endif",
        "",
    ]
    for name, data in programs.items():
        ident = name.upper()
        lines.append(f"static const uint8_t BR_DEMO_{ident}[{len(data)}] PROGMEM = {{")
        for offset in range(0, len(data), 16):
            row = ", ".join(f"0x{x:02X}" for x in data[offset : offset + 16])
            lines.append("  " + row + ("," if offset + 16 < len(data) else ""))
        lines.append("};")
        lines.append(f"static constexpr size_t BR_DEMO_{ident}_SIZE = sizeof(BR_DEMO_{ident});")
        source = (EXAMPLES / f"demo-{name}.asm").read_text()
        code_bytes = 0
        for raw in source.splitlines():
            line = raw.split(";", 1)[0].split("#", 1)[0].strip()
            if not line:
                continue
            while ":" in line and line.split(":", 1)[0].replace("_", "A").isalnum():
                line = line.split(":", 1)[1].strip()
            if line and line.split()[0].upper() in assembler.TABLE:
                code_bytes += 2
        lines.append(f"static constexpr size_t BR_DEMO_{ident}_CODE_BYTES = {code_bytes};")
        lines.append("")
    return "\n".join(lines)


def generated() -> dict[str, bytes]:
    return {
        "audio": assembler.assemble((EXAMPLES / "demo-audio.asm").read_text(), "AUDIO"),
        "video": assembler.assemble((EXAMPLES / "demo-video.asm").read_text(), "VIDEO"),
    }


def generated_diagnostic() -> bytes:
    return assembler.assemble((EXAMPLES / "audio-440-diagnostic.asm").read_text(), "AUDIO")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fail if generated artifacts differ")
    args = parser.parse_args()
    programs = generated()
    if any(len(program) != 256 for program in programs.values()):
        raise SystemExit("demo bank must be exactly 256 bytes")
    header = render_header(programs)
    paths = {name: EXAMPLES / f"demo-{name}.bin" for name in programs}
    diagnostic = generated_diagnostic()
    diagnostic_path = EXAMPLES / "audio-440-diagnostic.bin"
    if len(diagnostic) != 256:
        raise SystemExit("audio diagnostic bank must be exactly 256 bytes")
    if args.check:
        stale = [str(path.relative_to(ROOT)) for name, path in paths.items() if not path.exists() or path.read_bytes() != programs[name]]
        if not diagnostic_path.exists() or diagnostic_path.read_bytes() != diagnostic:
            stale.append(str(diagnostic_path.relative_to(ROOT)))
        if not HEADER.exists() or HEADER.read_text() != header:
            stale.append(str(HEADER.relative_to(ROOT)))
        if stale:
            raise SystemExit("generated demo artifacts are stale: " + ", ".join(stale))
        print("PASS: AUDIO/VIDEO demo binaries and firmware arrays plus 440 Hz diagnostic match the ISA assembler")
        return 0
    for name, path in paths.items():
        path.write_bytes(programs[name])
    diagnostic_path.write_bytes(diagnostic)
    HEADER.write_text(header)
    print("Wrote examples/demo-audio.bin, examples/demo-video.bin and demo_programs.h")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
