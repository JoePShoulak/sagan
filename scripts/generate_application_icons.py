#!/usr/bin/env python3
"""Build the dependency-free macOS ICNS asset from the canonical PNG."""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "application" / "linux" / "sagan.png"
OUTPUT = ROOT / "assets" / "application" / "macos" / "sagan.icns"


def main() -> None:
    png = SOURCE.read_bytes()
    if not png.startswith(b"\x89PNG\r\n\x1a\n"):
        raise SystemExit(f"Canonical application icon is not a PNG: {SOURCE}")
    # ic09 is the standard 512x512 PNG-backed ICNS element.
    element = b"ic09" + struct.pack(">I", len(png) + 8) + png
    payload = b"icns" + struct.pack(">I", len(element) + 8) + element
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(payload)
    print(OUTPUT)


if __name__ == "__main__":
    main()
