#!/bin/sh

set -e

outdir="$(realpath $2)"
cd "$(dirname $0)"

./gen-trigo.py

cpu="$1"
srcs=$(find . -type f -name '*.c')

if test -n "$DEBUG"; then
  extra_flags="-ggdb"
fi

for src in $srcs; do
  obj=$(basename -s ".c" $src).o
  echo "compiling $src -> $obj for $cpu"
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
