#!/usr/bin/env python3

import struct
from os import path
from writeutil import write_file, RESOURCE_DIR

# read fotn file
with open(path.join(RESOURCE_DIR, "zap-vga16.psf"), "rb") as f:
    raw = f.read()

header, char_data = (raw[:4], raw[4:])
# parse PSF1 header
magic, font_mode, char_size = struct.unpack("<HBB", header)
# ensure magic is correct
assert magic == 0x436, "invalid PSF1 magic"

# compute the number of (ascii) characters in the font
num_chars = min(len(char_data) // char_size, 256)

# generate defines
font_h = f"""
#ifndef UI_H_
#error "include ui.h instead of font.h"
#endif

#define FONT_WIDTH  8
#define FONT_HEIGHT {char_size}

#ifdef DEFINE_FONT

static const char g_font[{num_chars}][FONT_HEIGHT] = {{
"""

# generate font data
for i in range(num_chars):
    c_data = char_data[(i * char_size) : ((i + 1) * char_size)]
    font_h += f"  [{i}] = {{\n"
    for b in c_data:
        font_h += f"    0b{b:08b},\n"
    font_h += "  },\n"

# generate footer
font_h += """
};

#endif // DEFINE_FONT
"""

write_file("font.h", font_h)
