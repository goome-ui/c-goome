#!/usr/bin/env python3
# makes src/wl_proto.h: the protocol code wl.c needs, with no wayland headers and every *_interface renamed gm_*
# usage: protocols/gen.py <wayland-scanner> <wayland.xml>
import re, subprocess, sys, tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

here = Path(__file__).resolve().parent
scanner, core = sys.argv[1], sys.argv[2]
keep = {"wl_display", "wl_registry", "wl_callback", "wl_compositor", "wl_shm_pool", "wl_shm", "wl_buffer", "wl_surface",
        "wl_region", "wl_seat", "wl_pointer", "wl_keyboard", "wl_touch", "wl_output"}
xmls = [core] + [str(here / n) for n in ("wlr-layer-shell-unstable-v1.xml", "wlr-screencopy-unstable-v1.xml", "xdg-output-unstable-v1.xml")]

heads, codes, names, notes = [], [], [], []
with tempfile.TemporaryDirectory(dir=here) as tmp:
    for i, x in enumerate(xmls):
        tree = ET.parse(x)
        root = tree.getroot()
        if i == 0:
            for itf in list(root.findall("interface")):
                if itf.get("name") not in keep:
                    root.remove(itf)
        for itf in root.findall("interface"):
            names.append(itf.get("name"))
        c = root.find("copyright")
        if c is not None:
            notes.append(root.get("name") + ":\n" + "\n".join(l.strip() for l in c.text.strip().splitlines()))
        src = Path(tmp) / f"p{i}.xml"
        tree.write(src)
        for mode, out in (("client-header", heads), ("private-code", codes)):
            r = subprocess.run([scanner, mode, str(src), "/dev/stdout"], check=True, capture_output=True, text=True)
            out.append(r.stdout)

def clean(t):
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    lines, skip = [], 0
    src = [l.rstrip() for l in t.splitlines()]
    while src and not src[-1].strip(): src.pop()
    if src and src[-1].strip() == "#endif" and any(re.match(r"#ifndef \w+_CLIENT_PROTOCOL_H$", l.strip()) for l in src): src.pop()
    for l in src:
        s = l.strip()
        if skip:
            if s.startswith("#endif"): skip = 0
            continue
        if re.match(r"#(ifndef|define) \w+_CLIENT_PROTOCOL_H$", s) or s.startswith(("#include", "#define WL_PRIVATE")): continue
        if re.match(r"#ifdef\s+__cplusplus", s) or s.startswith(("#ifndef __has_attribute", "#if (__has_attribute")):
            skip = 1
            continue
        lines.append(l)
    t = "\n".join(lines)
    t = re.sub(r"\bWL_PRIVATE\b", "GM_WL_PRIVATE", t)
    t = re.sub(r"\b(" + "|".join(names + ["xdg_popup"]) + r")_interface\b", r"gm_\1_interface", t)
    t = re.sub(r"\b((?:wayland|wlr_layer_shell_unstable_v1|wlr_screencopy_unstable_v1|xdg_output_unstable_v1)_types)\b", r"gm_\1", t)
    return t

body = "\n".join(clean(h) for h in heads) + "\n" + "\n".join(clean(c) for c in codes)
body += "\n\n// layer-shell get_popup points at it, we never make popups\nGM_WL_PRIVATE const struct wl_interface gm_xdg_popup_interface = {\"xdg_popup\", 1, 0, NULL, 0, NULL};"
body = re.sub(r"\n\s*\n+", "\n\n", body).strip()
top = "// made by protocols/gen.py from wayland-scanner output, dont edit\n/*\n" + "\n\n".join(notes) + "\n*/\n"
(here.parent / "src/wl_proto.h").write_text(top + "#ifndef GM_WL_PROTO_H\n#define GM_WL_PROTO_H\n\n#if defined(__GNUC__) && __GNUC__ >= 4\n#define GM_WL_PRIVATE __attribute__((visibility(\"hidden\")))\n#else\n#define GM_WL_PRIVATE\n#endif\n\n" + body + "\n\n#endif\n")
print("wrote src/wl_proto.h,", len(names), "interfaces")
