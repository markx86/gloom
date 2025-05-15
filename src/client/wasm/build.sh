#!/bin/sh

set -e

origdir=$PWD
wasmdir="$(realpath $(dirname $0))"

# delete all object files in dist directory
find $origdir -type f -name '*.o' -delete

cd $wasmdir

# generate font.h file
$wasmdir/tools/gen-font.py
# generate cosine table
$wasmdir/tools/gen-cos-table.py

out=gloom.wasm
outpath="$origdir/$out"

# glob .c files
srcs=$(find $wasmdir -type f -name '*.c')

# add debug flags
if test -n "$DEBUG"; then
  extra_flags="-ggdb"
fi

# build and link
clang \
  --target=wasm32 \
  -Wall \
  -Wextra \
  -I$wasmdir/include \
  -I$wasmdir/gen \
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
  $srcs

if test -n "$DEBUG"; then
  echo "generating source map..."
  llvm-dwarfdump -debug-info -debug-line --recurse-depth=0 $outpath.full > $outpath.dwarf
  ./tools/wasm-sourcemap.py $outpath.full -w $outpath -s -p $PWD -u http://localhost:4444/$out.map -o $outpath.map --dwarfdump-output=$outpath.dwarf
else
  mv $outpath.full $outpath
fi

