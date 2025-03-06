#!/bin/sh

set -e

outdir="$(realpath $2)"
cd "$(dirname $0)"

./gen-cos-table.py

cpu="$1"
srcs=$(find . -type f -name '*.c')

if test -n "$DEBUG"; then
  extra_flags="-ggdb"
fi

for src in $srcs; do
  obj=$(basename -s ".c" $src).o
  echo "compiling $(realpath $src) -> $(realpath $outdir/$obj) for $cpu"
  clang \
    --target=$cpu \
    -Wall \
    -Wextra \
    -I. \
    -O3 \
    -flto \
    -fno-builtin \
    -nostdlib \
    $extra_flags \
    -c \
    -o "$outdir/$obj" \
    $src
done
