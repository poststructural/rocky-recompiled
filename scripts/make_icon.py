"""Draws the app icon: a gold boxing glove on a dark red badge, in the
colours of the game's title. Writes assets/rocky.ico (16-256, PNG entries)
and assets/rocky-icon.png (256x256).

    python scripts/make_icon.py   (needs Pillow)
"""
import os
import sys
from PIL import Image, ImageChops, ImageDraw, ImageFilter

S = 1024  # drawn at 1024 and scaled down
ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def gradient(size, top, bottom):
    g = Image.new("RGBA", (1, size))
    for y in range(size):
        t = y / (size - 1)
        g.putpixel((0, y), tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)) + (255,))
    return g.resize((size, size))


def badge():
    mask = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(mask)
    d.rounded_rectangle((0, 0, S - 1, S - 1), radius=S // 10, fill=255)
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    img.paste(gradient(S, (150, 22, 18), (40, 4, 4)), (0, 0), mask)
    ImageDraw.Draw(img).rounded_rectangle((0, 0, S - 1, S - 1), radius=S // 10,
                                          outline=(230, 190, 90, 255), width=S // 32)
    return img


def smooth(points, steps=24):
    """Closed Catmull-Rom curve through points."""
    out = []
    n = len(points)
    for i in range(n):
        p0, p1, p2, p3 = (points[(i + k) % n] for k in (-1, 0, 1, 2))
        for s in range(steps):
            t = s / steps
            out.append(tuple(0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t +
                                    (-a + 3 * b - 3 * c + d) * t ** 3)
                             for a, b, c, d in zip(p0, p1, p2, p3)))
    return out


# A right glove seen from the thumb side: fist at the top right, cuff at the bottom.
HAND = [(330, 720), (262, 560), (268, 380), (350, 235), (500, 160), (660, 165),
        (790, 250), (845, 400), (815, 540), (735, 615), (640, 655), (625, 720)]
THUMB = [(585, 650), (590, 545), (660, 470), (770, 440), (865, 470), (870, 545),
         (790, 615), (680, 665)]
CUFF = (300, 690, 660, 885)


def shape_mask(*polys, rects=()):
    m = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(m)
    for p in polys:
        d.polygon(smooth(p), fill=255)
    for r in rects:
        d.rounded_rectangle(r, radius=45, fill=255)
    return m


def draw():
    img = badge()
    body = shape_mask(HAND, THUMB, rects=[CUFF])
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    shadow.paste((0, 0, 0, 255), (int(S * 0.03), int(S * 0.035)), body)
    img.alpha_composite(shadow)
    img.paste(gradient(S, (255, 236, 160), (200, 150, 40)), (0, 0), body)
    d = ImageDraw.Draw(img)
    line = (110, 60, 8, 255)
    w = S // 56
    # the thumb's edge, the line where the fingers fold, and the cuff
    seam = smooth(THUMB)
    d.line(seam[24:24 * 5], fill=line, width=w, joint="curve")
    d.line((CUFF[0] + 10, CUFF[1] + 10, CUFF[2] - 10, CUFF[1] + 10), fill=line, width=w)
    d.rectangle((CUFF[0], 770, CUFF[2], 820), fill=(150, 22, 18, 255))
    hl = Image.new("L", (S, S), 0)
    ImageDraw.Draw(hl).ellipse((380, 190, 700, 330), fill=120)
    hl = ImageChops.multiply(hl.filter(ImageFilter.GaussianBlur(S // 36)), body)
    img.paste((255, 250, 225, 255), (0, 0), hl)
    return img


def main():
    big = draw()
    sizes = [16, 24, 32, 48, 64, 128, 256]
    assets = os.path.join(ROOT, "assets")
    os.makedirs(assets, exist_ok=True)
    big.resize((256, 256), Image.LANCZOS).save(os.path.join(assets, "rocky-icon.png"))
    big.resize((256, 256), Image.LANCZOS).save(os.path.join(assets, "rocky.ico"),
                                               sizes=[(s, s) for s in sizes])
    print("wrote", os.path.join(assets, "rocky.ico"), "and rocky-icon.png")


if __name__ == "__main__":
    main()
