#!/usr/bin/env python3
"""Render the existing code-native launcher chevron as opaque Vita PNGs."""
import argparse
from pathlib import Path
import struct
import zlib
from generate_uwp_assets import chunk, pixel

ASSETS = {
    "sce_sys/icon0.png": (128, 128),
    "sce_sys/livearea/contents/bg.png": (840, 500),
    "sce_sys/livearea/contents/startup.png": (280, 158),
}


def png(width, height):
    colours = [(8, 21, 40), (244, 151, 36), (255, 238, 186)]
    indices = {rgb: index for index, rgb in enumerate(colours)}
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.append(indices[pixel(x, y, width, height)[:3]])
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0))
            + chunk(b"PLTE", bytes(value for rgb in colours for value in rgb))
            + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def generate(output):
    for name, (width, height) in ASSETS.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(png(width, height))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    generate(parser.parse_args().output)
