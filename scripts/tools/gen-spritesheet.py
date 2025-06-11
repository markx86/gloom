#!/usr/bin/env python3

try:
    import imageio.v3 as iio
    import numpy as np
except:
    print("Install ImageIO with `pip3 install iio`")
    exit(-1)

from os import path
from writeutil import write_file, RESOURCE_DIR

ss_data = iio.imread(path.join(RESOURCE_DIR, "player-sheet.png"))

height, width, _ = ss_data.shape

SPRITE_W = 57
SPRITE_H = 59

assert width % SPRITE_W == 0, width
assert height % SPRITE_H == 0, height

n_sprites_w = width // SPRITE_W
n_sprites_h = height // SPRITE_H
n_sprites = n_sprites_h * n_sprites_w

flat = np.frombuffer(ss_data.tobytes(), dtype=np.uint32)

transparency = flat[0]

colors_set = set(flat)
colors_set.remove(transparency)
colors = [transparency]  # ensure the transparency color is @ index 0
colors.extend(list(colors_set))  # color table

new_image = []
for c in flat:
    new_image.append(colors.index(c))

player_sprites_c = f"""
#include <types.h>

#define PLAYER_TILE_W {SPRITE_W}
#define PLAYER_TILE_H {SPRITE_H}

#define PLAYER_SPRITESHEET_W {width}
#define PLAYER_SPRITESHEET_H {height}

#define PLAYER_NTILES_W {n_sprites_w}
#define PLAYER_NTILES_H {n_sprites_h}
#define PLAYER_NTILES   {n_sprites}

#define PLAYER_COLTAB_LEN {len(colors)}

static const u32 player_coltab[PLAYER_COLTAB_LEN] = {{
"""

for c in colors:
    player_sprites_c += f"  {hex(c & 0xFFFFFF)},\n"

player_sprites_c += """
};

static const u8 player_spritesheet[PLAYER_NTILES * PLAYER_TILE_W * PLAYER_TILE_H] = {
"""

v_stride = SPRITE_H * width

for y in range(n_sprites_h):
    for x in range(n_sprites_w):
        for yy in range(SPRITE_H):
            for xx in range(SPRITE_W):
                off = (y * SPRITE_H + yy) * width + (x * SPRITE_W + xx)
                player_sprites_c += f"0x{colors.index(flat[off]):02x}, "
            player_sprites_c += "\n"

player_sprites_c += "};\n"

write_file("player-sprites.c", player_sprites_c)
