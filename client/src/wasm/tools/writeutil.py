import sys
from os import path, makedirs

SCRIPT_DIR = path.dirname(path.abspath(sys.argv[0]))
OUTPUT_DIR = path.join(SCRIPT_DIR, "..", "gen")
    
makedirs(OUTPUT_DIR, exist_ok=True)

def write_file(name: str, content: str | bytes):
    open_options = "w" if isinstance(content, str) else "wb"
    with open(path.join(OUTPUT_DIR, name), open_options) as f:
        f.write(content)

