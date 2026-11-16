#!/usr/bin/env python3
# makes tests/colorfonts.h, tiny made up fonts for the emoji + rtl tests. needs fontTools and PIL
import io
import struct
from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable
from fontTools.ttLib.tables.DefaultTable import DefaultTable
from fontTools.ttLib.tables import otTables as ot
from PIL import Image, ImageDraw

here = Path(__file__).resolve().parent


def shape(rects=(), polys=()):
    pen = TTGlyphPen(None)
    for x0, y0, x1, y1 in rects:
        pen.moveTo((x0, y0)), pen.lineTo((x0, y1)), pen.lineTo((x1, y1)), pen.lineTo((x1, y0)), pen.closePath()
    for pts in polys:
        pen.moveTo(pts[0])
        for p in pts[1:]:
            pen.lineTo(p)
        pen.closePath()
    return pen.glyph()


def arabic(h, w, conn_r, conn_l, dot):
    r = [(250 - w // 2, 0, 250 + w // 2, h)]
    if conn_r:
        r.append((250, 0, 500, 60))
    if conn_l:
        r.append((0, 0, 250, 60))
    if dot:
        r.append((200, -160, 300, -60))
    return shape(r)


def gsub_lig(ligs, order):
    st = ot.LigatureSubst()
    st.ligatures = {}
    for comps, out in ligs:
        lig = ot.Ligature()
        lig.LigGlyph, lig.Component = out, comps[1:]
        st.ligatures.setdefault(comps[0], []).append(lig)
    lk = ot.Lookup()
    lk.LookupType, lk.LookupFlag, lk.SubTable, lk.SubTableCount = 4, 0, [st], 1
    g = ot.GSUB()
    g.Version = 0x00010000
    g.LookupList = ot.LookupList()
    g.LookupList.Lookup, g.LookupList.LookupCount = [lk], 1
    fr = ot.FeatureRecord()
    fr.FeatureTag = "ccmp"
    fr.Feature = ot.Feature()
    fr.Feature.FeatureParams, fr.Feature.LookupListIndex, fr.Feature.LookupCount = None, [0], 1
    g.FeatureList = ot.FeatureList()
    g.FeatureList.FeatureRecord, g.FeatureList.FeatureCount = [fr], 1
    ls = ot.LangSys()
    ls.LookupOrder, ls.ReqFeatureIndex, ls.FeatureIndex, ls.FeatureCount = None, 0xFFFF, [0], 1
    sc = ot.Script()
    sc.DefaultLangSys, sc.LangSysRecord, sc.LangSysCount = ls, [], 0
    sr = ot.ScriptRecord()
    sr.ScriptTag, sr.Script = "DFLT", sc
    g.ScriptList = ot.ScriptList()
    g.ScriptList.ScriptRecord, g.ScriptList.ScriptCount = [sr], 1
    t = newTable("GSUB")
    t.table = g
    return t


def color_font():
    glyphs = {".notdef": shape([(50, 0, 450, 700)]), "space": shape()}
    cmap, adv = {0x20: "space"}, {".notdef": 500, "space": 300}

    def add(name, cp, g, a=600):
        glyphs[name], adv[name] = g, a
        if cp:
            cmap[cp] = name

    add("a", 0x61, shape([(80, 0, 480, 480)]))
    add("b", 0x62, shape([(80, 0, 480, 400), (80, 0, 180, 720)]))
    add("c", 0x63, shape(polys=[[(80, 0), (280, 520), (480, 0)]]))
    add("one", 0x31, shape([(240, 0, 360, 700)]), 500)
    add("two", 0x32, shape([(100, 0, 400, 120), (280, 0, 400, 700)]), 500)
    add("paren_l", 0x28, shape([(100, -100, 170, 760), (100, -100, 300, -40), (100, 700, 300, 760)]), 400)
    add("paren_r", 0x29, shape([(230, -100, 300, 760), (100, -100, 300, -40), (100, 700, 300, 760)]), 400)
    add("alef_he", 0x5D0, shape(polys=[[(60, 0), (440, 640), (520, 640), (140, 0)]], rects=[(80, 300, 200, 640)]), 580)
    add("bet_he", 0x5D1, shape([(60, 0, 520, 80), (420, 0, 520, 640), (60, 560, 520, 640)]), 580)
    add("gimel_he", 0x5D2, shape([(260, 0, 360, 640)], polys=[[(60, 0), (260, 300), (260, 200), (160, 0)]]), 420)
    letters = {0x628: ("beh", 300, 120, (0xFE8F, 0xFE90, 0xFE91, 0xFE92)), 0x644: ("lam", 720, 80, (0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0)),
               0x645: ("meem", 220, 220, (0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4)), 0x627: ("alef", 650, 60, (0xFE8D, 0xFE8E, 0, 0))}
    for base, (nm, h, w, forms) in letters.items():
        add(nm, base, arabic(h, w, 0, 0, 1), 500)
        for k, (cp, cr, cl) in enumerate(((forms[0], 0, 0), (forms[1], 1, 0), (forms[2], 0, 1), (forms[3], 1, 1))):
            if cp:
                add(f"{nm}_{k}", cp, arabic(h, w, cr, cl, k == 0), 500)
    add("lamalef", 0xFEFB, shape(polys=[[(60, 0), (120, 700), (180, 700), (160, 80), (380, 80), (440, 700), (500, 700), (460, 0)]]), 560)
    add("lamalef_f", 0xFEFC, shape(polys=[[(60, 0), (120, 700), (180, 700), (160, 80), (380, 80), (440, 700), (500, 700), (460, 0)]], rects=[(460, 0, 560, 60)]), 560)
    add("heart", 0x2764, shape(polys=[[(300, 0), (60, 400), (180, 620), (300, 480), (420, 620), (540, 400)]]))
    add("zwj", 0x200D, shape(), 0)
    sq = [(60, -40, 740, 640)]
    add("smile", 0x1F600, shape(sq), 800)
    add("tone", 0x1F3FB, shape(sq), 800)
    for n in ("face", "eyes", "mouth", "smile_tone", "twin", "heart_c"):
        add(n, 0, shape(sq), 800)
    glyphs["face"] = shape(polys=[[(60, 300), (160, 540), (400, 640), (640, 540), (740, 300), (640, 60), (400, -40), (160, 60)]])
    glyphs["eyes"] = shape([(230, 330, 330, 480), (470, 330, 570, 480)])
    glyphs["mouth"] = shape(polys=[[(200, 200), (400, 80), (600, 200), (560, 240), (400, 150), (240, 240)]])
    glyphs["heart_c"] = glyphs["heart"]
    order = list(glyphs)
    fb = FontBuilder(1000, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap, uvs=[(0x2764, 0xFE0F, "heart_c")])
    fb.setupGlyf({n: glyphs[n] for n in order})
    fb.setupHorizontalMetrics({n: (adv[n], 0) for n in order})
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
    fb.setupNameTable({"familyName": "goome test color", "styleName": "Regular"})
    fb.setupPost()
    fb.setupCOLR({"smile": [("face", 0), ("eyes", 1), ("mouth", 0xFFFF)], "smile_tone": [("face", 2), ("eyes", 1), ("mouth", 1)],
                  "tone": [("face", 2)], "twin": [("face", 0), ("eyes", 3)], "heart_c": [("heart_c", 3)]}, version=0)
    fb.setupCPAL([[(1.0, 0.8, 0.1, 1.0), (0.2, 0.15, 0.1, 1.0), (0.75, 0.5, 0.35, 1.0), (0.9, 0.1, 0.2, 1.0)]])
    fb.font["GSUB"] = gsub_lig([(["smile", "tone"], "smile_tone"), (["smile", "zwj", "smile"], "twin")], order)
    out = io.BytesIO()
    fb.font.save(out)
    return out.getvalue()


def cat_png(size):
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    s = size / 32
    d.polygon([(3 * s, 12 * s), (6 * s, 2 * s), (12 * s, 9 * s)], fill=(240, 150, 40, 255))
    d.polygon([(29 * s, 12 * s), (26 * s, 2 * s), (20 * s, 9 * s)], fill=(240, 150, 40, 255))
    d.ellipse([3 * s, 7 * s, 29 * s, 30 * s], fill=(250, 170, 60, 255))
    d.ellipse([9 * s, 14 * s, 13 * s, 19 * s], fill=(40, 30, 20, 255))
    d.ellipse([19 * s, 14 * s, 23 * s, 19 * s], fill=(40, 30, 20, 255))
    d.polygon([(14 * s, 21 * s), (18 * s, 21 * s), (16 * s, 23 * s)], fill=(230, 80, 120, 255))
    out = io.BytesIO()
    im.save(out, "PNG", optimize=True)
    return out.getvalue()


def bitmap_font(kind):
    order = [".notdef", "space", "cat"]
    fb = FontBuilder(1000, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap({0x20: "space", 0x1F431: "cat"})
    fb.setupGlyf({n: shape() for n in order})
    fb.setupHorizontalMetrics({".notdef": (500, 0), "space": (300, 0), "cat": (1100, 0)})
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
    fb.setupNameTable({"familyName": f"goome test {kind}", "styleName": "Regular"})
    fb.setupPost()
    png = cat_png(32)
    font = fb.font
    if kind == "cbdt":
        cbdt = struct.pack(">HH", 3, 0)
        img_off = len(cbdt)
        cbdt += struct.pack(">BBbbB", 32, 32, 1, 26, 34) + struct.pack(">I", len(png)) + png
        sub = struct.pack(">HHI", 1, 17, 0) + struct.pack(">II", img_off, len(cbdt))
        arr = struct.pack(">HHI", 2, 2, 8)
        line = struct.pack(">bbBbbbbbbbbb", 26, -6, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        size = struct.pack(">IIII", 8 + 48, len(arr) + len(sub), 1, 0) + line + line + struct.pack(">HHBBBb", 2, 2, 32, 32, 32, 1)
        cblc = struct.pack(">HHI", 3, 0, 1) + size + arr + sub
        for tag, data in (("CBDT", cbdt), ("CBLC", cblc)):
            t = DefaultTable(tag)
            t.data = data
            font[tag] = t
        del font["glyf"], font["loca"]
    else:
        n = len(order)
        rec = struct.pack(">hh4s", 1, -6, b"png ") + png
        offs = [4 + 4 * (n + 1)] * 3 + [4 + 4 * (n + 1) + len(rec)]
        strike = struct.pack(">HH", 32, 72) + b"".join(struct.pack(">I", o) for o in offs) + rec
        t = DefaultTable("sbix")
        t.data = struct.pack(">HHII", 1, 1, 1, 12) + strike
        font["sbix"] = t
    out = io.BytesIO()
    font.save(out)
    return out.getvalue()


def carr(name, data):
    rows = [",".join(f"0x{b:02x}" for b in data[i:i + 24]) for i in range(0, len(data), 24)]
    return f"static const unsigned char {name}[{len(data)}] = {{\n    " + ",\n    ".join(rows) + ",\n};\n"


fonts = [("colr_ttf", color_font()), ("cbdt_ttf", bitmap_font("cbdt")), ("sbix_ttf", bitmap_font("sbix"))]
text = "// made by tests/mkfonts.py, tiny made up fonts: colr_ttf has COLR/CPAL emoji, hebrew, arabic forms, a ccmp ligature and a cmap 14 heart. cbdt_ttf + sbix_ttf hold one png cat\n"
text += "".join(carr(n, d) for n, d in fonts)
(here / "colorfonts.h").write_text(text)
for n, d in fonts:
    print(n, len(d))
