#!/usr/bin/env python3
# glues include/goome.h + src/*.c into one goome.h you can drop into any project
import re, sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
order = ["goome.c", "draw.c", "image.c", "font.c", "ui.c", "test.c", "x11.c", "wl.c", "gnome.c", "win32.c", "macos.c"]
seen = set()

def inline(path):
    out = []
    for line in path.read_text().splitlines():
        m = re.match(r'\s*#\s*include\s+"([^"]+)"', line)
        if not m:
            out.append(line)
            continue
        name = m.group(1)
        if name == "goome.h":
            continue
        dep = next((d / name for d in (path.parent, root / "src", root / "include") if (d / name).exists()), None)
        if dep is None:
            sys.exit(f"{path}: cant find {name}")
        if dep in seen:
            continue
        seen.add(dep)
        out += inline(dep)
    return out

head = (root / "include/goome.h").read_text().rstrip().splitlines()
body = []
for name in order:
    p = root / "src" / name
    seen.add(p)
    body += inline(p)

ver = re.search(r'GOOME_VERSION "([^"]+)"', "\n".join(head)).group(1)
top = [
    f"// goome {ver}, one file version. https://github.com/goome-ui/c-goome (GPL-3.0)",
    "// in exactly one .c file, before any other include:",
    "//   #define GOOME_IMPLEMENTATION",
    "//   #include \"goome.h\"",
    "// everywhere else just #include \"goome.h\". made by tools/single.py, edit src/ instead",
    "",
]
text = "\n".join(top + head + ["", "#if defined(GOOME_IMPLEMENTATION) && !defined(GOOME_IMPLEMENTED)", "#define GOOME_IMPLEMENTED", ""] + body + ["", "#endif", ""])
out = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "goome.h"
out.write_text(text)
print(f"wrote {out} ({len(text) // 1024} KB)")
