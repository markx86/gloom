#!/bin/sh

set -e

# cd into project root
cd $(dirname $(realpath $0))/../

esbuild \
  --sourcemap \
  --bundle \
  --platform=node \
  --target=node10 \
  --outfile=dist/server.js \
  src/server/app.ts

chmod +x dist/server.js
