#!/bin/sh

set -e

# cd into project root
cd $(dirname $(realpath $0))/../

mkdir -p ./static/js
# build wasm
./scripts/build-wasm.sh ./static/js/gloom.wasm
# minify js 
esbuild --minify --bundle --sourcemap --outfile=static/js/bundle.min.js src/client/index.js
