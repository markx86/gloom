#!/usr/bin/env python3

PLAYER_SPRITE_W = 57
PLAYER_SPRITE_H = 59

BULLET_SPRITE_W = 12
BULLET_SPRITE_H = 12

try:
    import imageio.v3 as iio
    import numpy as np
except:
    print("Install ImageIO with `pip3 install iio`")
    exit(-1)

from os import path
from writeutil import write_file, RESOURCE_DIR

#################################
# PLAYER SPRITESHEET GENERATION #
#################################

ss_data = iio.imread(path.join(RESOURCE_DIR, "player-sheet.png"))
height, width, _ = ss_data.shape

assert width % PLAYER_SPRITE_W == 0, width
assert height % PLAYER_SPRITE_H == 0, height

n_sprites_w = width // PLAYER_SPRITE_W
n_sprites_h = height // PLAYER_SPRITE_H
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

sprites_c = f"""
#include <types.h>

#define PLAYER_TILE_W {PLAYER_SPRITE_W}
#define PLAYER_TILE_H {PLAYER_SPRITE_H}

#define PLAYER_SPRITESHEET_W {width}
#define PLAYER_SPRITESHEET_H {height}

#define PLAYER_NTILES_W {n_sprites_w}
#define PLAYER_NTILES_H {n_sprites_h}
#define PLAYER_NTILES   {n_sprites}

#define PLAYER_COLTAB_LEN {len(colors)}

static const u32 player_coltab[PLAYER_COLTAB_LEN] = {{
"""

for c in colors:
    sprites_c += f"  {hex(c & 0xFFFFFF)},\n"

sprites_c += """
};

static const u8 player_spritesheet[PLAYER_NTILES * PLAYER_TILE_W * PLAYER_TILE_H] = {
"""

v_stride = PLAYER_SPRITE_H * width

for y in range(n_sprites_h):
    for x in range(n_sprites_w):
        for yy in range(PLAYER_SPRITE_H):
            for xx in range(PLAYER_SPRITE_W):
                off = (y * PLAYER_SPRITE_H + yy) * width + (x * PLAYER_SPRITE_W + xx)
                sprites_c += f"0x{new_image[off]:02X}, "
            sprites_c += "\n"
sprites_c += "};\n"


#############################
# BULLET TEXTURE GENERATION #
#############################

bullet_data = iio.imread(path.join(RESOURCE_DIR, "bullet.png"))
height, width, _ = bullet_data.shape

assert width == BULLET_SPRITE_W, width
assert height == BULLET_SPRITE_H, height

flat = np.frombuffer(bullet_data.tobytes(), dtype=np.uint32)

transparency = flat[0]

colors_set = set(flat)
colors_set.remove(transparency)
colors = [transparency]  # ensure the transparency color is @ index 0
colors.extend(list(colors_set))  # color table

new_image = []
for c in flat:
    new_image.append(colors.index(c))

sprites_c += f"""
#define BULLET_TEXTURE_W {BULLET_SPRITE_W}
#define BULLET_TEXTURE_H {BULLET_SPRITE_H}

#define BULLET_COLTAB_LEN {len(colors)}

static const u32 bullet_coltab[BULLET_COLTAB_LEN] = {{
"""

for c in colors:
    sprites_c += f"  {hex(c & 0xFFFFFF)},\n"

sprites_c += """
};

static const u8 bullet_texture[BULLET_TEXTURE_W * BULLET_TEXTURE_H] = {
"""

for c in new_image:
    sprites_c += f"0x{c:02X}, "
sprites_c += "\n};\n"

write_file("sprites.c", sprites_c)
