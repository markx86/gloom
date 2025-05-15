#!/bin/sh

set -e

# cd into project root
cd $(dirname $(realpath $0))/../

mkdir -p ./static/js
# build wasm
./scripts/build-wasm.sh ./static/js/gloom.wasm
# minify js 
cat ./src/client/app.js | esbuild --minify --loader=js > ./static/js/gloom.min.js
