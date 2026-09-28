#!/usr/bin/env python3
"""Generate compact Unicode identifier tables from official UCD data."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def read_property(path: Path, name: str) -> list[tuple[int, int]]:
    ranges: list[tuple[int, int]] = []
    pattern = re.compile(r"^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s*;\s*([^\s#]+)")
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if not match or match.group(3) != name:
            continue
        begin = int(match.group(1), 16)
        end = int(match.group(2) or match.group(1), 16)
        ranges.append((begin, end))
    if not ranges:
        raise RuntimeError(f"property {name!r} was not found in {path}")
    return ranges


def emit_array(name: str, ranges: list[tuple[int, int]]) -> str:
    entries = ",\n".join(f"    range{{0x{begin:X}, 0x{end:X}}}" for begin, end in ranges)
    return f"inline constexpr std::array<range, {len(ranges)}> {name}{{{{\n{entries}\n}}}};"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("derived_core_properties", type=Path)
    parser.add_argument("emoji_data", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    content = "\n\n".join(
        [
            "// Generated from Unicode 17.0.0 UCD data. Do not edit by hand.\n"
            "#pragma once\n\n#include <array>\n#include <cstdint>\n\n"
            "namespace sagan::unicode::tables\n{\n"
            "  struct range\n  {\n    char32_t begin;\n    char32_t end;\n  };",
            emit_array("xid_start", read_property(args.derived_core_properties, "XID_Start")),
            emit_array("xid_continue", read_property(args.derived_core_properties, "XID_Continue")),
            emit_array("extended_pictographic", read_property(args.emoji_data, "Extended_Pictographic")),
            emit_array("emoji_modifier", read_property(args.emoji_data, "Emoji_Modifier")),
        ]
    )
    args.output.write_text(content + "\n}\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
