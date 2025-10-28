#!/usr/bin/env python3

PLAYER_SPRITE_W = 57
PLAYER_SPRITE_H = 59

BULLET_SPRITE_W = 12
BULLET_SPRITE_H = 12

try:
    import imageio.v3 as iio
    import numpy as np
except:
    print("Install ImageIO with `pip3 install imageio`")
    exit(-1)

from os import path
from writeutil import write_file, RESOURCE_DIR


def load_image(image_name, transparency_color=None):
    image_data = iio.imread(path.join(RESOURCE_DIR, f"{image_name}.png"))
    height, width, channels = image_data.shape
    assert channels == 4
    # convert image_data to flat array of uint32s
    image_data = np.frombuffer(image_data.tobytes(), np.uint32)
    # generate color table
    color_table = list(set(image_data))
    if transparency_color is not None:
        # if we have a transparency color, make sure it is
        # at index 0 in the color table
        assert transparency_color in color_table
        color_table.remove(transparency_color)
        color_table.insert(0, transparency_color)
    # ensure the image has less than 256 colors, so that we can
    # fit every index in one byte
    assert len(color_table) < 256
    # convert RGBA colors to color table indices
    image_indexed = [color_table.index(x) for x in image_data]
    return width, height, image_indexed, color_table


def gen_player_spritesheet():
    width, height, image_indexed, color_table = load_image(
        "player-sheet", transparency_color=0xFFFFFF00
    )

    # ensure the width is divisible by the player sprite width
    assert width % PLAYER_SPRITE_W == 0, width
    # ensure the height is divisible by the player sprite height
    assert height % PLAYER_SPRITE_H == 0, height

    n_sprites_w = width // PLAYER_SPRITE_W
    n_sprites_h = height // PLAYER_SPRITE_H
    n_sprites = n_sprites_h * n_sprites_w

    # generate defines
    csrc = f"""
#define PLAYER_TILE_W {PLAYER_SPRITE_W}
#define PLAYER_TILE_H {PLAYER_SPRITE_H}

#define PLAYER_SPRITESHEET_W {width}
#define PLAYER_SPRITESHEET_H {height}

#define PLAYER_NTILES_W {n_sprites_w}
#define PLAYER_NTILES_H {n_sprites_h}
#define PLAYER_NTILES   {n_sprites}

#define PLAYER_COLTAB_LEN {len(color_table)}

static const u32 g_player_coltab[PLAYER_COLTAB_LEN] = {{
"""

    # generate color table for the spritesheet
    for c in color_table:
        c &= 0xFFFFFF  # discard alpha channel
        csrc += f"  {hex(c)},\n"

    csrc += """
};

static const u8 g_player_spritesheet[PLAYER_NTILES * PLAYER_TILE_W * PLAYER_TILE_H] = {
  """

    # generate image data
    for y in range(n_sprites_h):
        for x in range(n_sprites_w):
            for yy in range(PLAYER_SPRITE_H):
                for xx in range(PLAYER_SPRITE_W):
                    off = (y * PLAYER_SPRITE_H + yy) * width + (
                        x * PLAYER_SPRITE_W + xx
                    )
                    csrc += f"0x{image_indexed[off]:02X}, "
                csrc += "\n  "
    csrc += "};\n"

    return csrc


def gen_bullet_texture():
    width, height, image_indexed, color_table = load_image(
        "bullet", transparency_color=0xFFFF00FF
    )

    # ensure the width and height are correct
    assert width == BULLET_SPRITE_W, width
    assert height == BULLET_SPRITE_H, height

    # generate defines
    csrc = f"""
#define BULLET_TEXTURE_W {BULLET_SPRITE_W}
#define BULLET_TEXTURE_H {BULLET_SPRITE_H}

#define BULLET_COLTAB_LEN {len(color_table)}

static const u32 g_bullet_coltab[BULLET_COLTAB_LEN] = {{
"""

    # generate color table for bullet texture
    for i in color_table:
        i &= 0xFFFFFF  # discard alpha channel
        csrc += f"  {hex(i)},\n"

    csrc += """
};

static const u8 g_bullet_texture[BULLET_TEXTURE_W * BULLET_TEXTURE_H] = {
  """

    # generate image data for bullet texture
    for i in image_indexed:
        csrc += f"0x{i:02X}, "
    csrc += "\n};\n"

    return csrc


sprites_c = f"""
#include <gloom/types.h>
{gen_player_spritesheet()}
{gen_bullet_texture()}
"""

write_file("sprites.c", sprites_c)
