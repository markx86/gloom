#!/bin/sh

set -e

origdir=$PWD
wasmdir="$(realpath $(dirname $0))"
gamedir="$wasmdir/../../../game"

cd $wasmdir

$gamedir/build.sh wasm32 $origdir

out=gloom.wasm
outpath="$origdir/$out"

srcs=$(find $wasmdir -type f -name '*.c')
objs=$(find $origdir -type f -name '*.o')

if test -n "$DEBUG"; then
  extra_flags="-ggdb"
fi

clang \
  --target=wasm32 \
  -Wall \
  -Wextra \
  -I$gamedir \
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
  -o $outpath.full \
  $srcs $objs

if test -n "$DEBUG"; then
  echo "generating source map..."
  llvm-dwarfdump -debug-info -debug-line --recurse-depth=0 $outpath.full > $outpath.dwarf
  ./tools/wasm-sourcemap.py $outpath.full -w $outpath -s -p $PWD -u http://localhost:4444/$out.map -o $outpath.map --dwarfdump-output=$outpath.dwarf
else
  mv $outpath.full $outpath
fi

