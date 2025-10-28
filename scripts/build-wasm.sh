#!/bin/sh

set -e

# cd into project root
cd $(dirname $(realpath $0))/../

outpath="$1"
shift
extra_flags="$@"
outfile=$(basename $outpath)
wasmdir="./src/client/wasm"
gendir=$(mktemp --directory)

mkdir -p $gendir

# generate font.h file
./scripts/tools/gen-font.py $gendir
# generate cosine table
./scripts/tools/gen-cos-table.py $gendir
# generate sprites.c file
./scripts/tools/gen-sprites.py $gendir

# glob .c files
srcs=$(find $wasmdir -type f -name '*.c')

# build and link
clang \
  --target=wasm32 \
  -Wall \
  -Wextra \
  -I$wasmdir/include \
  -I$gendir \
  -O3 \
  -flto \
  -fno-builtin \
  -nostdlib \
  -Wl,--lto-O3 \
  -Wl,--no-entry \
  -Wl,--export-all \
  -Wl,--allow-undefined-file=$wasmdir/platform.syms \
  $extra_flags \
  -o $outpath \
  $srcs
