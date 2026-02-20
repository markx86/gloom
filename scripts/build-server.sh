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

chmod +x build/server.js
