#!/bin/sh

set -e

origdir=$PWD
wasmdir="$(realpath $(dirname $0))"

cd $wasmdir

../../game/build.sh wasm32 .

out=$origdir/gloom.wasm
srcs=$(find $wasmdir -type f -name '*.c')
objs=$(find $wasmdir -type f -name '*.o')

if test -n "$DEBUG"; then
  extra_flags="-ggdb"
fi

clang \
  --target=wasm32 \
  -Wall \
  -Wextra \
  -I../../game \
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
  $srcs $objs

if test -n "$DEBUG"; then
  echo "generating source map..."
  llvm-dwarfdump -debug-info -debug-line --recurse-depth=0 $out.full > $out.dwarf
  ./tools/wasm-sourcemap.py $out.full -w $out -s -p $PWD -u /$out.map -o $out.map --dwarfdump-output=$out.dwarf
else
  mv $out.full $out
fi

