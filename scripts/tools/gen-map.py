#!/usr/bin/env python3

try:
    import imageio.v3 as iio
    import numpy as np
except:
    print("Install ImageIO with `pip3 install iio`", file=sys.stderr)
    exit(-1)

import sys
from pathlib import Path


def get_value(px):
    return 1 if px == 0xFFFFFFFF else 0


def main():
    if len(sys.argv) != 2:
        print(f"USAGE: {sys.argv[0]} IMAGE", file=sys.stderr)
        exit(-1)
    map_name = "".join(
        f"{x[0].upper()}{x[1:]}" for x in Path(sys.argv[1]).stem.split("-")[1:]
    )
    image_data = iio.imread(sys.argv[1])

    width, height, channels = image_data.shape
    assert channels == 4

    raw_bytes = image_data.tobytes()
    image_data = np.frombuffer(raw_bytes, dtype=np.uint32)

    print(f"const map{map_name} = new GameMap({width}, {height}, [")

    line = "  "
    w = 0
    for px in image_data:
        line += f"{get_value(px)}, "
        w += 1
        if w == width:
            print(line)
            height -= 1
            w = 0
            line = "  "
    assert height == 0

    print("""], [
  // TODO
]);
""")


if __name__ == "__main__":
    main()
