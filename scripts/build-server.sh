#!/bin/sh

set -e

# cd into project root
cd $(dirname $(realpath $0))/../

esbuild \
  --sourcemap \
  --bundle \
  --minify \
  --platform=node \
  --target=node10 \
  --outfile=build/server.js \
  src/server/app.ts

# copy over the require node module for sqlite3
cp node_modules/sqlite3/build/Release/*.node build/

chmod +x build/server.js
