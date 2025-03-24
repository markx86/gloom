#!/usr/bin/env python3

import sys
import subprocess
import base64
import re
from os import makedirs
from os.path import dirname, abspath
from pathlib import Path

release_mode = "--release" in sys.argv

here_dir = Path(abspath(dirname(sys.argv[0])))
src_dir = here_dir / ".." / "src";
dist_dir = here_dir / ".." / "dist";
wasm_dir = src_dir / "wasm";

makedirs(dist_dir, exist_ok=True)

subprocess.check_call(wasm_dir / "build.sh", cwd=dist_dir)

wasm = open(dist_dir / "gloom.wasm", "rb").read()
wasm_b64 = base64.b64encode(wasm)

app_js = open(src_dir / "app.js", "rb").read()
app_js = app_js.replace(b"@@WASMB64@@", wasm_b64)

if release_mode:
    app_min_js = subprocess.check_output(["esbuild", "--minify", "--loader=js"], input=app_js)
else:
    app_min_js = app_js

html = open(src_dir / "app.html", "r").read()
if release_mode:
    html_min = re.sub(r">\s+<", "><", html).encode()
else:
    html_min = html.encode()
html_min = html_min.replace(b"@@JAVASCRIPT@@", app_min_js)

open(dist_dir / "index.html", "wb").write(html_min)
