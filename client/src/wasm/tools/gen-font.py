#!/usr/bin/env python3

import struct
import os
import sys

abs_path = os.path.realpath(sys.argv[0])
work_dir = os.path.dirname(abs_path)

with open(os.path.join(work_dir, "zap-vga16.psf"), "rb") as f:
    raw = f.read()

header, char_data = (raw[:4], raw[4:])

magic, font_mode, char_size = struct.unpack("<HBB", header)

assert magic == 0x436, "invalid PSF1 magic"

num_chars = min(len(char_data) // char_size, 256)

font_h = f"""
#ifndef __DRAW_H__
#error "include draw.h instead of font.h"
#endif

#define FONT_WIDTH  8
#define FONT_HEIGHT {char_size}

#ifdef DEFINE_FONT

static const char __font[{num_chars}][FONT_HEIGHT] = {{
"""

for i in range(num_chars):
    c_data = char_data[(i * char_size):((i+1) * char_size)]
    font_h += f"  [{i}] = {{\n"
    for b in c_data:
        font_h += f"    0b{b:08b},\n"
    font_h += "  },\n"

font_h += """
};

#endif // DEFINE_FONT
"""

with open(os.path.join(work_dir, "..", "font.h"), "w") as f:
    f.write(font_h)
