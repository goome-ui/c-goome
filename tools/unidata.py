#!/usr/bin/env python3
# makes src/unidata.h: bidi classes, mirror pairs and arabic forms, from pythons unicodedata
import unicodedata as u
from pathlib import Path

root = Path(__file__).resolve().parent.parent
names = ["L", "R", "AL", "EN", "ES", "ET", "AN", "CS", "NSM", "BN", "B", "S", "WS", "ON", "LRE", "LRO", "RLE", "RLO", "PDF", "LRI", "RLI", "FSI", "PDI"]
idx = {n: i for i, n in enumerate(names)}

runs, prev = [], None
for c in range(0x110000):
    b = u.bidirectional(chr(c)) or "L"
    if b != prev:
        runs.append((c << 5) | idx[b])
        prev = b

swaps = [("LEFT", "RIGHT"), ("LESS-THAN", "GREATER-THAN"), ("SUBSET", "SUPERSET"), ("ELEMENT OF", "CONTAINS AS MEMBER"), ("PRECEDES", "SUCCEEDS"), ("OPENING", "CLOSING")]
mirror = {}
for c in range(0x110000):
    ch = chr(c)
    if not u.mirrored(ch) or not u.name(ch, ""):
        continue
    n = u.name(ch)
    for a, b in swaps:
        for x, y in ((a, b), (b, a)):
            if x not in n:
                continue
            m = n.replace(x, "\0").replace(y, x).replace("\0", y)
            try:
                d = ord(u.lookup(m))
            except KeyError:
                continue
            if d != c and u.mirrored(chr(d)):
                mirror[c] = d

forms, lamalef = {}, {}
slot = {"<isolated>": 0, "<final>": 1, "<initial>": 2, "<medial>": 3}
for c in list(range(0xFB50, 0xFE00)) + list(range(0xFE70, 0xFF00)):
    d = u.decomposition(chr(c)).split()
    if len(d) < 2 or d[0] not in slot:
        continue
    base = [int(x, 16) for x in d[1:]]
    if len(base) == 1 and 0x0600 <= base[0] < 0x0800:
        f = forms.setdefault(base[0], [0, 0, 0, 0])
        if not f[slot[d[0]]]:
            f[slot[d[0]]] = c
    elif len(base) == 2 and base[0] == 0x0644 and 0xFEF5 <= c <= 0xFEFC:
        lamalef.setdefault(base[1], [0, 0])[slot[d[0]]] = c

emoji = [(0x231A, 0x231B), (0x23E9, 0x23EC), (0x23F0, 0x23F0), (0x23F3, 0x23F3), (0x25FD, 0x25FE), (0x2614, 0x2615), (0x2648, 0x2653),
         (0x267F, 0x267F), (0x2693, 0x2693), (0x26A1, 0x26A1), (0x26AA, 0x26AB), (0x26BD, 0x26BE), (0x26C4, 0x26C5), (0x26CE, 0x26CE),
         (0x26D4, 0x26D4), (0x26EA, 0x26EA), (0x26F2, 0x26F3), (0x26F5, 0x26F5), (0x26FA, 0x26FA), (0x26FD, 0x26FD), (0x2705, 0x2705),
         (0x270A, 0x270B), (0x2728, 0x2728), (0x274C, 0x274C), (0x274E, 0x274E), (0x2753, 0x2755), (0x2757, 0x2757), (0x2795, 0x2797),
         (0x27B0, 0x27B0), (0x27BF, 0x27BF), (0x2B1B, 0x2B1C), (0x2B50, 0x2B50), (0x2B55, 0x2B55)]


def rows(vals, per, fmt):
    out = []
    for i in range(0, len(vals), per):
        out.append("    " + ",".join(fmt(v) for v in vals[i:i + per]) + ",")
    return "\n".join(out)


lines = ["#ifndef GOOME_UNIDATA_H", "#define GOOME_UNIDATA_H", "#include <stdint.h>", "",
         f"// made by tools/unidata.py from unicode {u.unidata_version}",
         "enum { " + ", ".join(f"UD_{n}" for n in names) + " };",
         f"static const uint32_t ud_bidi[{len(runs)}] = {{", rows(runs, 10, lambda v: f"0x{v:x}"), "};"]
mp = sorted(mirror.items())
lines += [f"static const uint32_t ud_mirror[{len(mp) * 2}] = {{", rows([x for p in mp for x in p], 12, lambda v: f"0x{v:x}"), "};"]
fs = sorted(forms.items())
lines += [f"static const uint16_t ud_forms[{len(fs) * 5}] = {{", rows([x for k, v in fs for x in [k] + v], 10, lambda v: f"0x{v:x}"), "};"]
la = sorted(lamalef.items())
lines += [f"static const uint16_t ud_lamalef[{len(la) * 3}] = {{", rows([x for k, v in la for x in [k] + v], 12, lambda v: f"0x{v:x}"), "};"]
lines += [f"static const uint16_t ud_emoji[{len(emoji) * 2}] = {{", rows([x for p in emoji for x in p], 12, lambda v: f"0x{v:x}"), "};"]
lines += ["#endif", ""]
(root / "src" / "unidata.h").write_text("\n".join(lines))
print(f"{len(runs)} bidi runs, {len(mp)} mirrors, {len(fs)} arabic letters, {len(la)} lam alefs")
