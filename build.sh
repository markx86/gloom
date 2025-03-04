#!/bin/sh

set -e

./tools/gen-trigo.py

out=gloom.wasm

srcs=$(find . -type f -name '*.c')

if test -n "$DEBUG"; then
  extra_flags="-ggdb"
fi

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
  $extra_flags \
  -o $out.full \
  $srcs

if test -n "$DEBUG"; then
  echo "generating source map..."
  llvm-dwarfdump -debug-info -debug-line --recurse-depth=0 $out.full > $out.dwarf
  ./tools/wasm-sourcemap.py $out.full -w $out -s -p $PWD -u /$out.map -o $out.map --dwarfdump-output=$out.dwarf
else
  mv $out.full $out
fi

