#!/usr/bin/env python3
# makes tests/images.h: tiny images in every format for tests/unit.c, with the pixel hash pillow decodes them to
# needs pillow, only rerun it when you change the image set
import io, random, struct, zlib
from pathlib import Path
from PIL import Image, ImageDraw

rnd = random.Random(42)
CH = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}
PASSES = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]


def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)


def pack(row, ct, depth):
    if depth < 8:
        out, per = bytearray(), 8 // depth
        for i in range(0, len(row), per):
            b = 0
            for j, v in enumerate(row[i:i + per]):
                b |= v[0] << (8 - depth * (j + 1))
            out.append(b)
        return bytes(out)
    return b"".join(struct.pack(">H", v) if depth == 16 else bytes([v]) for p in row for v in p)


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else b if pb <= pc else c


def filt(rows, bpp):
    out, prev = bytearray(), bytes(len(rows[0]))
    for n, r in enumerate(rows):
        ft = n % 5
        out.append(ft)
        for i in range(len(r)):
            a = r[i - bpp] if i >= bpp else 0
            c = prev[i - bpp] if i >= bpp else 0
            out.append((r[i] - [0, a, prev[i], (a + prev[i]) // 2, paeth(a, prev[i], c)][ft]) & 255)
        prev = r
    return bytes(out)


def png(w, h, ct, depth, px, pal=None, trns=None, interlace=0, level=9, strategy=0, splits=1):
    bpp = max(1, CH[ct] * depth // 8)
    if interlace:
        raw = b""
        for x0, y0, dx, dy in PASSES:
            sub = [r for r in ([px[y][x] for x in range(x0, w, dx)] for y in range(y0, h, dy)) if r]
            if sub:
                raw += filt([pack(r, ct, depth) for r in sub], bpp)
    else:
        raw = filt([pack(r, ct, depth) for r in px], bpp)
    co = zlib.compressobj(level, zlib.DEFLATED, 15, 9, strategy)
    z = co.compress(raw) + co.flush()
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, depth, ct, 0, 0, interlace))
    out += chunk(b"tEXt", b"Comment\0skip me")
    if pal:
        out += chunk(b"PLTE", bytes(v for p in pal for v in p))
    if trns is not None:
        out += chunk(b"tRNS", trns)
    step = (len(z) + splits - 1) // splits
    for i in range(0, len(z), step):
        out += chunk(b"IDAT", z[i:i + step])
    return out + chunk(b"IEND", b"")


def pixels(w, h, ct, depth, npal=0):
    mx = (1 << depth) - 1
    return [[(rnd.randrange(npal),) if ct == 3 else tuple(rnd.randrange(mx + 1) for _ in range(CH[ct])) for _ in range(w)] for _ in range(h)]


def photo(w, h):
    i = Image.new("RGB", (w, h), (30, 60, 90))
    d = ImageDraw.Draw(i)
    for _ in range(6):
        x, y, r = rnd.randrange(w), rnd.randrange(h), rnd.randrange(2, 8)
        d.ellipse([x - r, y - r, x + r, y + r], fill=(rnd.randrange(256), rnd.randrange(256), rnd.randrange(256)))
    return i


def save(img, fmt, **kw):
    b = io.BytesIO()
    img.save(b, fmt, **kw)
    return b.getvalue()


def bmp(w, h, bpp, rows, pal=b"", masks=None, top=False):
    stride = ((w * bpp + 31) // 32) * 4
    data = b"".join(r + bytes(stride - len(r)) for r in (rows if top else rows[::-1]))
    comp = 3 if masks else 0
    info = struct.pack("<IiiHHIIiiII", 108 if masks else 40, w, -h if top else h, 1, bpp, comp, len(data), 2835, 2835, len(pal) // 4, 0)
    if masks:
        info += struct.pack("<IIII", *masks) + bytes(108 - 56)
    off = 14 + len(info) + len(pal)
    return b"BM" + struct.pack("<IHHI", off + len(data), 0, 0, off) + info + pal + data


def premul(r, g, b, a):
    f = lambda c: (2 * c * a + 255) // 510
    return a << 24 | f(r) << 16 | f(g) << 8 | f(b)


def fnv(px):
    v = 2166136261
    for p in px:
        for k in range(4):
            v = ((v ^ ((p >> (k * 8)) & 255)) * 16777619) & 0xFFFFFFFF
    return v


def ref(data):
    i = Image.open(io.BytesIO(data))
    i.load()
    if i.format == "JPEG":
        from PIL import ImageOps
        i = ImageOps.exif_transpose(i)
    if i.mode.startswith("I"):
        return i.size, fnv(premul(*(round(v * 255 / 65535),) * 3, 255) for v in i.getdata())
    return i.size, fnv(premul(*p) for p in i.convert("RGBA").getdata())


imgs = []
imgs.append(("png gray 1 bit", png(9, 3, 0, 1, pixels(9, 3, 0, 1))))
pal = [(rnd.randrange(256), rnd.randrange(256), rnd.randrange(256)) for _ in range(16)]
imgs.append(("png palette 4 bit trns interlaced", png(6, 6, 3, 4, pixels(6, 6, 3, 4, 16), pal, bytes(range(0, 256, 23)), interlace=1)))
imgs.append(("png rgb 16 bit", png(5, 4, 2, 16, pixels(5, 4, 2, 16))))
imgs.append(("png gray alpha stored interlaced", png(5, 5, 4, 8, pixels(5, 5, 4, 8), interlace=1, level=0)))
imgs.append(("png rgba fixed huffman split", png(8, 6, 6, 8, pixels(8, 6, 6, 8), strategy=zlib.Z_FIXED, splits=3)))
g = pixels(7, 4, 0, 8)
imgs.append(("png gray key", png(7, 4, 0, 8, g, trns=struct.pack(">H", g[1][2][0]))))
imgs.append(("png rgba 16 bit interlaced", png(9, 9, 6, 16, pixels(9, 9, 6, 16), interlace=1)))
imgs.append(("jpeg 420", save(photo(16, 16), "JPEG", quality=75, subsampling=2)))
imgs.append(("jpeg 422 restarts", save(photo(13, 9), "JPEG", quality=90, subsampling=1, restart_marker_blocks=1)))
imgs.append(("jpeg 444", save(photo(10, 10), "JPEG", quality=95, subsampling=0)))
imgs.append(("jpeg progressive 420 restarts", save(photo(14, 11), "JPEG", quality=85, subsampling=2, progressive=True, restart_marker_blocks=2)))
imgs.append(("jpeg gray", save(photo(9, 7).convert("L"), "JPEG", quality=80)))
ex = Image.Exif()
ex[0x0112] = 6
imgs.append(("jpeg exif rotated", save(photo(10, 6), "JPEG", quality=80, exif=ex.tobytes())))
pal4 = [(rnd.randrange(256), rnd.randrange(256), rnd.randrange(256)) for _ in range(16)]
rows = [bytes(rnd.randrange(256) for _ in range(3)) for _ in range(3)]
imgs.append(("bmp 4 bit", bmp(5, 3, 4, rows, b"".join(bytes([b, g, r, 0]) for r, g, b in pal4))))
rows = [b"".join(struct.pack("<I", rnd.randrange(1 << 32)) for _ in range(3)) for _ in range(2)]
imgs.append(("bmp 32 bit alpha top down", bmp(3, 2, 32, rows, masks=(0xFF0000, 0xFF00, 0xFF, 0xFF000000), top=True)))
imgs.append(("bmp 24 bit", save(photo(5, 3), "BMP")))
imgs.append(("ppm", save(photo(3, 2), "PPM")))
imgs.append(("pgm 16 bit", b"P5\n# hi\n3 1\n65535\n" + bytes([0, 0, 0x80, 0x7F, 0xFF, 0xFF])))
imgs.append(("qoi", save(Image.merge("RGBA", [photo(6, 4).convert("L")] * 3 + [Image.linear_gradient("L").resize((6, 4))]), "QOI")))

rows = []
for name, data in imgs:
    (w, h), hv = ref(data)
    rows.append((name, data, w, h, hv))

base = save(photo(8, 8), "JPEG")
cut = base.index(b"\xff\xc0")
arith = base[:cut] + b"\xff\xc9" + base[cut + 2:cut + 2 + struct.unpack(">H", base[cut + 2:cut + 4])[0]]

out = ["// made by tests/mkimages.py, dont edit", "", "typedef struct {", "    const char *name;", "    const unsigned char *data;",
       "    unsigned n;", "    int w, h;", "    uint32_t hash;", "} img_case;", ""]


def arr(name, data):
    lines = ["static const unsigned char %s[%d] = {" % (name, len(data))]
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join(str(b) for b in data[i:i + 24]) + ",")
    return lines + ["};", ""]


for k, r in enumerate(rows):
    out += arr("img_%d" % k, r[1])
out += arr("img_arith", arith)
out.append("static const img_case img_cases[] = {")
for k, (name, data, w, h, hv) in enumerate(rows):
    out.append('    {"%s", img_%d, %d, %d, %d, 0x%08xu},' % (name, k, len(data), w, h, hv))
out += ["};", ""]
Path(__file__).with_name("images.h").write_text("\n".join(out))
print("wrote tests/images.h,", sum(len(r[1]) for r in rows) + len(arith), "bytes of images")
