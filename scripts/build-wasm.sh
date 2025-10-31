#!/bin/sh

set -e

# cd into project root
cd $(dirname $(realpath $0))/../

outpath="$1"
shift
extra_flags="$@"
outfile=$(basename $outpath)
wasmdir="./src/client/wasm"

# glob .c files
srcs=$(find $wasmdir/gloom-core/src -type f -name '*.c')

# build and link
clang \
  --target=wasm32 \
  -Wall \
  -Wextra \
  -I$wasmdir/gloom-core/include \
  -I$wasmdir/gloom-core/gen \
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
