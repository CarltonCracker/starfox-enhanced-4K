#!/usr/bin/env python3
"""Render our existing launcher chevron as opaque iPhone/iPad app icons."""
import argparse
from pathlib import Path
import struct
import zlib
from generate_uwp_assets import chunk, pixel

ICONS = {
    'AppIcon60@2x.png': 120,
    'AppIcon60@3x.png': 180,
    'AppIcon76.png': 76,
    'AppIcon76@2x.png': 152,
    'AppIcon83.5@2x.png': 167,
}


def generate(output):
    output.mkdir(parents=True, exist_ok=True)
    for name, size in ICONS.items():
        rows = bytearray()
        for y in range(size):
            rows.append(0)
            for x in range(size):
                rows.extend(pixel(x, y, size, size)[:3])
        data = (b'\x89PNG\r\n\x1a\n'
                + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))
        (output / name).write_bytes(data)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    generate(parser.parse_args().output)
