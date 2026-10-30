#!/usr/bin/env python3
"""Convert the checked-in FujiNet NIO logo to the loader's ANTIC mode B bitmap.

The source is 160x96; each pair of rows becomes one 160-pixel monochrome row.
ANTIC mode B displays each row for two scanlines, restoring the source height.
The generated 960-byte file is checked in, so normal builds do not need Pillow.
"""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
image = Image.open(root / "images/config-nio-160x96x4.png").convert("RGB")
if image.size != (160, 96):
    raise ValueError("expected a 160x96 image")
bitmap = bytearray()
for row in range(48):
    for col in range(0, 160, 8):
        value = 0
        for bit in range(8):
            lit = any(max(image.getpixel((col + bit, row * 2 + y))) >= 128
                      for y in (0, 1))
            value = value * 2 + lit
        bitmap.append(value)
(root / "src/atari-bootstrap/assets/splash.bin").write_bytes(bitmap)
