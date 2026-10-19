#!/usr/bin/env python3
"""Which of chimera-padtest's button cells are lit.

chimera-padtest (main.c) draws, for each port, 16 cells, one per button,
lit (yellow) while the machine reads that button as held. This prints the
names of the cells that are lit in a screenshot.

Usage: cells.py <picture.tga> [port]     port 0 or 1, default 0

Any frame will do: a cell is drawn whole in every field, so the answer does
not depend on which half of an interlaced picture the frame shows.
SPDX-License-Identifier: MIT
"""
import sys

NAMES = ["Up", "Down", "Left", "Right", "Start", "Select", "Square", "Cross",
         "Circle", "Triangle", "L1", "R1", "L2", "R2", "L3", "R3"]


def read_tga(path):
    d = open(path, "rb").read()
    w = d[12] | (d[13] << 8)
    h = d[14] | (d[15] << 8)
    off = 18 + d[0]
    px = d[off:off + w * h * 4]
    rows = [px[y * w * 4:(y + 1) * w * 4] for y in range(h)]
    if not (d[17] & 0x20):
        rows.reverse()   # TGA rows are bottom-up unless the origin bit says otherwise
    return w, h, rows


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    port = int(sys.argv[2]) if len(sys.argv) == 3 else 0
    w, h, rows = read_tga(sys.argv[1])
    # the program draws at 640x448; a picture of another size is scaled to it
    sx, sy = w / 640.0, h / 448.0
    for i, name in enumerate(NAMES):
        cx = 40 + 72 * (i % 8) + 32
        cy = 40 + 200 * port + 56 * (i // 8) + 20
        x, y = int(cx * sx), int(cy * sy)
        b, g, r = rows[y][x * 4], rows[y][x * 4 + 1], rows[y][x * 4 + 2]
        if r > 0xc0 and g > 0x90 and b < 0x60:   # lit: (0xf0, 0xc0, 0x20)
            print(name)


if __name__ == "__main__":
    main()
