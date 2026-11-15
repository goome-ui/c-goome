#!/usr/bin/env python3
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parent.parent / "examples" / "assets"
SS = 8
WHITE = (255, 255, 255, 255)


def canvas(w, h):
    img = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def save(img, name, w, h):
    img.resize((w, h), Image.LANCZOS).save(OUT / (name + ".png"), optimize=True)


def s(v):
    return v * SS


def stroke(d, pts, width, col=WHITE):
    p = [(s(x), s(y)) for x, y in pts]
    d.line(p, fill=col, width=int(s(width)), joint="curve")
    r = s(width) / 2
    for x, y in (p[0], p[-1]):
        d.ellipse((x - r, y - r, x + r, y + r), fill=col)


def chevron(name, rot):
    img, d = canvas(32, 32)
    pts = [(10, 12), (16, 19), (22, 12)]
    c = 16
    a = math.radians(rot)
    pts = [(c + (x - c) * math.cos(a) - (y - c) * math.sin(a), c + (x - c) * math.sin(a) + (y - c) * math.cos(a)) for x, y in pts]
    stroke(d, pts, 4)
    save(img, name, 32, 32)


def tri(name, rot, size=32):
    img, d = canvas(size, size)
    c = size / 2
    pts = [(c - 8, c - 4), (c + 8, c - 4), (c, c + 6)]
    a = math.radians(rot)
    pts = [(c + (x - c) * math.cos(a) - (y - c) * math.sin(a), c + (x - c) * math.sin(a) + (y - c) * math.cos(a)) for x, y in pts]
    big = [(s(x), s(y)) for x, y in pts]
    d.polygon(big, fill=WHITE)
    d.line(big + [big[0]], fill=WHITE, width=int(s(2.5)), joint="curve")
    save(img, name, size, size)


def sparkle(d, cx, cy, r, inner, col=WHITE):
    pts = []
    for i in range(64):
        t = i / 64 * 2 * math.pi
        k = abs(math.cos(2 * t)) ** 2
        rr = inner + (r - inner) * k
        pts.append((s(cx + rr * math.cos(t)), s(cy + rr * math.sin(t))))
    d.polygon(pts, fill=col)


def rrect(d, box, r, fill=None, outline=None, width=1):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((s(x0), s(y0), s(x1), s(y1)), radius=s(r), fill=fill, outline=outline, width=int(s(width)) if outline else 0)


def vgrad(w, h, top, bot):
    g = Image.new("RGBA", (w * SS, h * SS))
    px = g.load()
    for y in range(h * SS):
        t = y / max(1, h * SS - 1)
        c = tuple(int(top[i] + (bot[i] - top[i]) * t + 0.5) for i in range(4))
        for x in range(w * SS):
            px[x, y] = c
    return g


def masked(fill, mask):
    out = Image.new("RGBA", fill.size, (0, 0, 0, 0))
    out.paste(fill, (0, 0), mask)
    return out


def shape_mask(w, h, draw):
    m = Image.new("L", (w * SS, h * SS), 0)
    draw(ImageDraw.Draw(m))
    return m


def skin_rr(name, w, h, r, top, bot, line, lip=None, inner=None, lw=1):
    m = shape_mask(w, h, lambda d: d.rounded_rectangle((0, 0, s(w) - 1, s(h) - 1), radius=s(r), fill=255))
    img = masked(vgrad(w, h, top, bot), m)
    d = ImageDraw.Draw(img)
    if lip:
        lm = shape_mask(w, h, lambda q: q.rounded_rectangle((0, 0, s(w) - 1, s(h) - 1), radius=s(r), fill=255))
        cut = shape_mask(w, h, lambda q: q.rounded_rectangle((0, 0, s(w) - 1, s(h) - s(1.5) - 1), radius=s(r), fill=255))
        lm = Image.composite(Image.new("L", lm.size, 0), lm, cut)
        img.paste(Image.new("RGBA", img.size, lip), (0, 0), lm)
    if inner:
        im = shape_mask(w, h, lambda q: q.rounded_rectangle((s(1), s(1), s(w) - s(1) - 1, s(h) - s(1) - 1), radius=s(r - 1), outline=255, width=s(1)))
        top_only = Image.new("L", im.size, 0)
        ImageDraw.Draw(top_only).rectangle((0, 0, s(w), s(h) // 2), fill=255)
        im = Image.composite(im, Image.new("L", im.size, 0), top_only)
        img.paste(Image.new("RGBA", img.size, inner), (0, 0), im)
    if line:
        d.rounded_rectangle((0, 0, s(w) - 1, s(h) - 1), radius=s(r), outline=line, width=int(s(lw)))
    save(img, name, w, h)


def titlebar(name, round_top):
    w, h, r = 32, 32, 10
    m = shape_mask(w, h, lambda d: d.rounded_rectangle((0, 0, s(w) - 1, s(h) + s(r) * 2), radius=s(r) if round_top else 0, fill=255))
    img = masked(vgrad(w, h, (246, 240, 253, 255), (229, 218, 247, 255)), m)
    d = ImageDraw.Draw(img)
    d.rectangle((0, s(h) - s(1), s(w), s(h)), fill=(196, 176, 232, 255))
    hl = shape_mask(w, h, lambda q: q.rounded_rectangle((0, 0, s(w) - 1, s(h) + s(r) * 2), radius=s(r) if round_top else 0, outline=255, width=s(1)))
    top_only = Image.new("L", hl.size, 0)
    ImageDraw.Draw(top_only).rectangle((0, 0, s(w), s(r)), fill=255)
    if round_top:
        img.paste(Image.new("RGBA", img.size, (255, 255, 255, 200)), (0, 0), Image.composite(hl, Image.new("L", hl.size, 0), top_only))
    else:
        d.rectangle((0, 0, s(w), s(1) - 1), fill=(255, 255, 255, 200))
    save(img, name, w, h)


def knob(name, size, body_top, body_bot, rim, dot):
    img = Image.new("RGBA", (size * SS, size * SS), (0, 0, 0, 0))
    c = size * SS / 2
    r = size * SS * 0.40
    sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse((c - r, c - r + s(1.5), c + r, c + r + s(1.5)), fill=(40, 20, 60, 110))
    sh = sh.filter(ImageFilter.GaussianBlur(s(1.2)))
    img.alpha_composite(sh)
    m = Image.new("L", img.size, 0)
    ImageDraw.Draw(m).ellipse((c - r, c - r, c + r, c + r), fill=255)
    body = masked(vgrad(size, size, body_top, body_bot), m)
    img.alpha_composite(body)
    d = ImageDraw.Draw(img)
    d.ellipse((c - r, c - r, c + r, c + r), outline=rim, width=int(s(1)))
    if dot:
        q = r * 0.36
        d.ellipse((c - q, c - q, c + q, c + q), fill=dot)
    save(img, name, size, size)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    chevron("arrow_down", 0)
    chevron("arrow_left", 90)
    chevron("arrow_up", 180)
    chevron("arrow_right", 270)
    tri("sort_down", 0)
    tri("sort_up", 180)
    tri("collapse", 0)
    tri("expand", -90)

    img, d = canvas(32, 32)
    stroke(d, [(7, 17), (13, 23), (25, 9)], 4.5)
    save(img, "check", 32, 32)

    img, d = canvas(32, 32)
    d.ellipse((s(8), s(8), s(24), s(24)), fill=WHITE)
    save(img, "radio_dot", 32, 32)

    img, d = canvas(32, 32)
    stroke(d, [(9, 9), (23, 23)], 3.6)
    stroke(d, [(23, 9), (9, 23)], 3.6)
    save(img, "close", 32, 32)

    img, d = canvas(32, 32)
    stroke(d, [(8, 16), (24, 16)], 3)
    save(img, "minimize", 32, 32)

    img, d = canvas(32, 32)
    rrect(d, (8, 8, 24, 24), 4, outline=WHITE, width=2.6)
    save(img, "maximize", 32, 32)

    img, d = canvas(32, 32)
    rrect(d, (12, 6, 26, 20), 3.5, outline=WHITE, width=2.4)
    rrect(d, (6, 12, 20, 26), 3.5, fill=(0, 0, 0, 0), outline=WHITE, width=2.4)
    m = shape_mask(32, 32, lambda q: q.rounded_rectangle((s(7.2), s(13.2), s(18.8), s(24.8)), radius=s(2.4), fill=255))
    img.paste(Image.new("RGBA", img.size, (0, 0, 0, 0)), (0, 0), m)
    save(img, "restore", 32, 32)

    img, d = canvas(32, 32)
    stroke(d, [(9, 16), (23, 16)], 3.4)
    save(img, "minus", 32, 32)
    stroke(d, [(16, 9), (16, 23)], 3.4)
    save(img, "plus", 32, 32)

    img, d = canvas(24, 24)
    for x, y in ((19, 19), (13, 19), (19, 13), (7, 19), (13, 13), (19, 7)):
        d.ellipse((s(x - 2), s(y - 2), s(x + 2), s(y + 2)), fill=WHITE)
    save(img, "resize", 24, 24)

    img, d = canvas(24, 48)
    for x in (8, 16):
        for y in (14, 24, 34):
            d.ellipse((s(x - 2.6), s(y - 2.6), s(x + 2.6), s(y + 2.6)), fill=WHITE)
    save(img, "drag_handle", 24, 48)

    img, d = canvas(36, 36)
    star = []
    for i in range(8):
        t = i / 8 * 2 * math.pi - math.pi / 2
        r = 15 if i % 2 == 0 else 5.5
        star.append((s(18 + r * math.cos(t)), s(18 + r * math.sin(t))))
    d.polygon(star, fill=WHITE)
    d.line(star + [star[0]], fill=WHITE, width=int(s(2)), joint="curve")
    save(img, "toast", 36, 36)

    img = Image.new("RGBA", (36 * SS, 36 * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = 18 * SS
    for r, w, col in ((14, 1.6, (40, 20, 60, 120)), (12.5, 3.2, WHITE)):
        d.ellipse((c - s(r), c - s(r), c + s(r), c + s(r)), outline=col, width=int(s(w)))
    save(img, "picker", 36, 36)

    knob("slider_knob", 36, (167, 128, 238, 255), (110, 66, 196, 255), (88, 50, 160, 255), (255, 255, 255, 235))
    knob("toggle_knob", 36, (255, 255, 255, 255), (238, 232, 246, 255), (200, 186, 222, 255), None)

    img, d = canvas(32, 32)
    heart = []
    for i in range(96):
        t = i / 96 * 2 * math.pi
        x = 16 * math.sin(t) ** 3
        y = -(13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t))
        heart.append((s(16 + x * 0.72), s(16.5 + y * 0.72)))
    d.polygon(heart, fill=WHITE)
    save(img, "heart", 32, 32)

    skin_rr("button", 24, 24, 7, (255, 255, 255, 255), (244, 238, 232, 255), (214, 203, 194, 255), lip=(200, 188, 178, 255))
    skin_rr("input", 24, 24, 7, (246, 241, 235, 255), (251, 248, 244, 255), (214, 203, 194, 255))
    skin_rr("checkbox", 36, 36, 11, (255, 255, 255, 255), (246, 241, 236, 255), (196, 180, 220, 255), lw=2)
    skin_rr("radio", 36, 36, 17.99, (255, 255, 255, 255), (246, 241, 236, 255), (196, 180, 220, 255), lw=2)
    skin_rr("toggle", 76, 44, 21.99, (224, 214, 238, 255), (236, 229, 245, 255), (200, 186, 222, 255), lw=2)
    skin_rr("slider", 16, 6, 2.99, (216, 206, 226, 255), (232, 225, 240, 255), None)
    skin_rr("slider_fill", 16, 6, 2.99, (168, 126, 240, 255), (118, 72, 204, 255), None)
    skin_rr("scrollbar", 6, 20, 2.99, (160, 124, 226, 150), (128, 88, 206, 150), None)
    skin_rr("tab", 24, 24, 7, (154, 112, 232, 255), (114, 70, 200, 255), (98, 58, 178, 255), inner=(255, 255, 255, 90))
    skin_rr("tooltip", 24, 24, 7, (255, 253, 248, 255), (250, 245, 238, 255), (190, 170, 222, 255))
    skin_rr("menu", 28, 28, 9, (255, 255, 255, 255), (251, 248, 245, 255), (214, 202, 228, 255))
    titlebar("titlebar", True)
    titlebar("titlebar_app", False)


if __name__ == "__main__":
    main()
