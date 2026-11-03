#!/usr/bin/env python3
# ppm helpers for tests/wayland.sh
#   bg W H out.png out.ppm      pattern picture (png for swaybg, ppm to compare with)
#   px f x y                    prints r g b
#   cmp a b [ax ay w h [bx by]] prints max channel diff and how many px differ by more than 2 (sizes must fit)
#   size f
import struct, sys, zlib

def load(path):
    d = open(path, "rb").read()
    vals, i = [], 2
    while len(vals) < 3:
        while d[i:i + 1].isspace(): i += 1
        if d[i:i + 1] == b"#":
            while d[i:i + 1] != b"\n": i += 1
            continue
        j = i
        while d[j:j + 1].isdigit(): j += 1
        vals.append(int(d[i:j])); i = j
    return vals[0], vals[1], d[i + 1:]

def pattern(x, y):
    return (x * 7 + y * 3) & 255, (y * 5 + (x >> 2)) & 255, ((x >> 5) * 40 + (y >> 5) * 70) & 255

cmd = sys.argv[1]
if cmd == "bg":
    w, h = int(sys.argv[2]), int(sys.argv[3])
    rows = [bytes(c for x in range(w) for c in pattern(x, y)) for y in range(h)]
    raw = b"".join(b"\0" + r for r in rows)
    def chunk(t, b): return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xffffffff)
    open(sys.argv[4], "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 1)) + chunk(b"IEND", b""))
    open(sys.argv[5], "wb").write(b"P6\n%d %d\n255\n" % (w, h) + b"".join(rows))
elif cmd == "px":
    w, h, p = load(sys.argv[2]); x, y = int(sys.argv[3]), int(sys.argv[4])
    k = (y * w + x) * 3
    print(p[k], p[k + 1], p[k + 2])
elif cmd == "size":
    w, h, _ = load(sys.argv[2]); print(w, h)
elif cmd == "cmp":
    aw, ah, a = load(sys.argv[2]); bw, bh, b = load(sys.argv[3])
    n = [int(v) for v in sys.argv[4:]]
    ax, ay, w, h = n[:4] if len(n) >= 4 else (0, 0, aw, ah)
    bx, by = n[4:6] if len(n) >= 6 else (0, 0)
    if ax + w > aw or ay + h > ah or bx + w > bw or by + h > bh:
        print("size mismatch %dx%d vs %dx%d" % (aw, ah, bw, bh)); sys.exit(1)
    worst, bad = 0, 0
    for y in range(h):
        ra = a[((ay + y) * aw + ax) * 3:((ay + y) * aw + ax + w) * 3]
        rb = b[((by + y) * bw + bx) * 3:((by + y) * bw + bx + w) * 3]
        if ra == rb: continue
        for i in range(0, w * 3, 3):
            d = max(abs(ra[i] - rb[i]), abs(ra[i + 1] - rb[i + 1]), abs(ra[i + 2] - rb[i + 2]))
            worst = max(worst, d)
            if d > 2: bad += 1
    print(worst, bad)
