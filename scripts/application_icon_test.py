#!/usr/bin/env python3
"""Validate canonical application-icon assets without platform GUI access."""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


png = (ROOT / "assets/application/linux/sagan.png").read_bytes()
require(png.startswith(b"\x89PNG\r\n\x1a\n"), "Linux application icon is not PNG")
require(struct.unpack(">II", png[16:24]) == (512, 512), "Linux application icon must be 512x512")

ico = (ROOT / "assets/application/windows/sagan.ico").read_bytes()
require(ico[:4] == b"\x00\x00\x01\x00", "Windows application icon is not ICO")
require(struct.unpack("<H", ico[4:6])[0] >= 7, "Windows ICO must retain its multi-size icon set")

icns = (ROOT / "assets/application/macos/sagan.icns").read_bytes()
require(icns[:4] == b"icns", "macOS application icon is not ICNS")
require(struct.unpack(">I", icns[4:8])[0] == len(icns), "macOS ICNS length is invalid")
require(icns[8:12] == b"ic09", "macOS ICNS is missing its 512x512 element")
require(icns[16:] == png, "macOS ICNS was not generated from the canonical PNG")

print("Canonical Windows, Linux, and macOS application icons validated.")
