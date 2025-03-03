#!/bin/sh

set -e

./gen-trigo.py

srcs=$(find . -type f -name '*.c')

clang \
  --target=wasm32 \
  -Wall \
  -Wextra \
  -I. \
  -O3 \
  -flto \
  -fno-builtin \
  -nostdlib \
  -Wl,--lto-O3 \
  -Wl,--no-entry \
  -Wl,--export-all \
  -Wl,--allow-undefined-file=env.syms \
  -o gloom.wasm \
  $srcs
