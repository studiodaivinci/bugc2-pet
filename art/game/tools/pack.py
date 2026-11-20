"""Packs the game's art into C arrays compiled into the firmware (no filesystem upload).

    python pack.py              # writes firmware/lib/game/src/assets.h and assets_data.cpp, prints sizes
    python pack.py --specimen   # also writes art/game/design/type_specimen.png at true size and 3x (not committed)

Inputs (art/game/):
  holes/art_NN.png     the picked page for hole NN (720x1280); falls back to holes/guide_NN.png
  sprites/<name>.png   RGBA sprites; animals are sprites/animal_<kind>.png in holes.ANIMALS order
  fonts/*.ttf          Fraunces and Nunito (OFL)

Formats (gfx.h): hole pages are 8-bit indexed with a 256-colour swapped-565 palette, LZ-packed; sprites are
swapped-565 plus an 8-bit alpha plane; fonts are 4-bit antialiased glyphs for ASCII 32..126.
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import holes  # noqa: E402

ART = Path(__file__).resolve().parents[1]
SRC = ART.parents[1] / "firmware" / "lib" / "game" / "src"
W, H = 135, 240

FRAUNCES = ART / "fonts" / "Fraunces[SOFT,WONK,opsz,wght].ttf"
NUNITO = ART / "fonts" / "Nunito[wght].ttf"
# name, file, pixel size, axes
FONTS = [
    ("kFontBig", FRAUNCES, 30, {"opsz": 72, "wght": 900, "SOFT": 100, "WONK": 0}),
    ("kFontTitle", FRAUNCES, 19, {"opsz": 36, "wght": 800, "SOFT": 100, "WONK": 0}),
    ("kFontBody", NUNITO, 13, {"wght": 800}),
    ("kFontSmall", NUNITO, 10, {"wght": 900}),
]

# Sprite sizes on screen (height in pixels).
ANIMAL_H = {"tortoise": 22, "rabbit": 24, "hedgehog": 20, "frog": 20, "duck": 22, "beaver": 24, "penguin": 24,
            "seal": 20, "polarbear": 30}


def swap565(r, g, b):
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return ((v >> 8) | (v << 8)) & 0xFFFF


def carray(name, ctype, values, per=16, fmt="{}"):
    lines = [f"const {ctype} {name}[] = {{"]
    for i in range(0, len(values), per):
        lines.append("    " + ", ".join(fmt.format(v) for v in values[i:i + per]) + ",")
    lines.append("};")
    return lines


# --- LZ: 0nnnnnnn = n+1 literals; 1nnnnnnn lo hi = copy n+3 from distance ---------------------------------------


def lz(data):
    out = bytearray()
    lits = bytearray()
    table = {}
    i, n = 0, len(data)

    def flush():
        nonlocal lits
        while lits:
            chunk = lits[:128]
            out.append(len(chunk) - 1)
            out.extend(chunk)
            lits = lits[128:]

    while i < n:
        best_len, best_d = 0, 0
        if i + 3 <= n:
            key = bytes(data[i:i + 3])
            for p in reversed(table.get(key, [])[-48:]):
                d = i - p
                if d > 65535:
                    break
                L = 3
                while L < 130 and i + L < n and data[p + L] == data[i + L]:
                    L += 1
                if L > best_len:
                    best_len, best_d = L, d
                    if L == 130:
                        break
        if best_len >= 3:
            flush()
            out += bytes([0x80 | (best_len - 3), best_d & 0xFF, best_d >> 8])
            for k in range(best_len):
                if i + k + 3 <= n:
                    table.setdefault(bytes(data[i + k:i + k + 3]), []).append(i + k)
            i += best_len
        else:
            if i + 3 <= n:
                table.setdefault(bytes(data[i:i + 3]), []).append(i)
            lits.append(data[i])
            i += 1
    flush()
    return bytes(out)


def unlz(packed, total):
    out = bytearray()
    i = 0
    while i < len(packed) and len(out) < total:
        t = packed[i]
        i += 1
        if t & 0x80:
            L, d = (t & 0x7F) + 3, packed[i] | (packed[i + 1] << 8)
            i += 2
            for _ in range(L):
                out.append(out[-d])
        else:
            out += packed[i:i + t + 1]
            i += t + 1
    return bytes(out)


# --- Hole pages ---------------------------------------------------------------------------------------------------


def page_small(path):
    im = Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS)
    return im.filter(ImageFilter.UnsharpMask(radius=0.8, percent=60, threshold=2))


def pack_page(im, name):
    q = im.quantize(256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    pal = q.getpalette()[:768]
    pal += [0] * (768 - len(pal))
    idx = q.tobytes()
    packed = lz(idx)
    assert unlz(packed, W * H) == idx, "LZ round trip failed"
    colours = [swap565(pal[3 * k], pal[3 * k + 1], pal[3 * k + 2]) for k in range(256)]
    lines = carray(f"{name}Pal", "uint16_t", colours, 16, "0x{:04X}")
    lines += carray(f"{name}Data", "uint8_t", list(packed), 24)
    return lines, len(packed) + 512


# --- Sprites ------------------------------------------------------------------------------------------------------


def sprite_from(path, height=None, width=None):
    im = Image.open(path).convert("RGBA")
    box = im.getchannel("A").point(lambda v: 255 if v > 20 else 0).getbbox()
    im = im.crop(box)
    if height:
        w = max(1, round(im.width * height / im.height))
        size = (w, height)
    else:
        size = (width, max(1, round(im.height * width / im.width)))
    # Resize premultiplied so edges don't pick up the transparent pixels' colour.
    r, g, b, a = im.split()
    pre = Image.merge("RGB", [Image.composite(c, Image.new("L", im.size, 0), a) for c in (r, g, b)])
    pre = pre.resize(size, Image.LANCZOS)
    a = a.resize(size, Image.LANCZOS)
    px = []
    for (pr, pg, pb), av in zip(pre.get_flattened_data(), a.get_flattened_data()):
        if av == 0:
            px.append((0, 0, 0))
        else:
            px.append(tuple(min(255, round(c * 255 / av)) for c in (pr, pg, pb)))
    out = Image.new("RGB", size)
    out.putdata(px)
    out = out.filter(ImageFilter.UnsharpMask(radius=0.6, percent=50, threshold=2))
    return out, a


def pack_sprite(name, rgb, alpha):
    w, h = rgb.size
    lines = carray(f"{name}Px", "uint16_t", [swap565(*p) for p in rgb.get_flattened_data()], 16, "0x{:04X}")
    lines += carray(f"{name}A", "uint8_t", list(alpha.get_flattened_data()), 24)
    return lines, (w, h), w * h * 3


def placeholder_sprite(kind, h):
    im = Image.new("RGBA", (h * 4, h * 4), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse([4, 4, h * 4 - 4, h * 4 - 4], fill=(200, 120, 60, 255), outline=(60, 30, 20, 255), width=6)
    return im


# --- Fonts --------------------------------------------------------------------------------------------------------


def load_font(path, size, axes):
    f = ImageFont.truetype(str(path), size)
    names = [ax["name"].decode() if isinstance(ax["name"], bytes) else ax["name"] for ax in f.get_variation_axes()]
    tags = {"Optical size": "opsz", "Weight": "wght", "Softness": "SOFT", "Wonky": "WONK", "Wonk": "WONK"}
    vals = []
    for n, ax in zip(names, f.get_variation_axes()):
        tag = tags.get(n, n)
        vals.append(axes.get(tag, ax["default"]))
    f.set_variation_by_axes(vals)
    return f


def pack_font(name, path, size, axes):
    f = load_font(path, size, axes)
    ascent, descent = f.getmetrics()
    cap = -f.getbbox("H", anchor="ls")[1]
    glyphs, bits = [], []
    for code in range(32, 128):
        ch = chr(code)
        # A typographic apostrophe for the ASCII one; 127 is a middle dot for separators.
        shown = "’" if ch == "'" else ("·" if code == 127 else ch)
        adv = round(f.getlength(shown))
        pad = size
        im = Image.new("L", (size * 3, size * 3), 0)
        ImageDraw.Draw(im).text((pad, pad * 2), shown, font=f, fill=255, anchor="ls")
        box = im.getbbox()
        if not box:
            glyphs.append((len(bits), 0, 0, 0, 0, adv))
            continue
        crop = im.crop(box)
        gw, gh = crop.size
        glyphs.append((len(bits), gw, gh, box[0] - pad, box[1] - pad * 2, adv))
        bits += [min(15, (v + 8) // 17) for v in crop.get_flattened_data()]
    if len(bits) % 2:
        bits.append(0)
    packed = [(bits[i] << 4) | bits[i + 1] for i in range(0, len(bits), 2)]
    lines = carray(f"{name}Bits", "uint8_t", packed, 24)
    lines.append(f"const gfx::Glyph {name}Glyphs[] = {{")
    for off, gw, gh, dx, dy, adv in glyphs:
        lines.append(f"    {{{off}, {gw}, {gh}, {dx}, {dy}, {adv}}},")
    lines.append("};")
    lines.append(f"}}  // namespace\nconst gfx::Font {name} = {{{ascent + descent}, {cap}, {name}Glyphs, {name}Bits}};\nnamespace {{")
    return lines, len(packed) + len(glyphs) * 10


def specimen():
    """True-size type specimen on a page colour, plus a 3x view."""
    im = Image.new("RGB", (W, H), (236, 228, 206))
    d = ImageDraw.Draw(im)
    y = 6
    samples = [("kFontBig", "3  12"), ("kFontTitle", "Hole in one!"), ("kFontTitle", "Bunny Bend"),
               ("kFontBody", "Player 2's turn"), ("kFontBody", "A rabbit's teeth"), ("kFontBody", "never stop growing!"),
               ("kFontSmall", "PAR 3   BEST 2   MAKER 3")]
    spec = {n: (p, s, a) for n, p, s, a in FONTS}
    for n, text in samples:
        p, s, a = spec[n]
        f = load_font(p, s, a)
        y += s + 4
        d.text((6, y), text, font=f, fill=(40, 34, 30), anchor="ls")
    big = im.resize((W * 3, H * 3), Image.NEAREST)
    sheet = Image.new("RGB", (W * 3 + W + 20, H * 3), (20, 20, 20))
    sheet.paste(big, (0, 0))
    sheet.paste(im, (W * 3 + 10, 0))
    out = ART / "design" / "type_specimen.png"
    sheet.save(out)
    print("specimen:", out)


def main():
    if "--specimen" in sys.argv:
        specimen()
    total = 0
    L = ["// Generated by art/game/tools/pack.py from the art in art/game/. Do not edit.",
         "// Fonts: Fraunces and Nunito (SIL Open Font License, art/game/fonts/OFL-*.txt).",
         "// Art: Qwen-Image 2.1 (Qwen Research License, non-commercial; not under this repo's MIT licence), art/game/.",
         '#include "assets.h"', "", "namespace game {", "", "namespace {", ""]
    # Hole pages.
    pages = []
    for n in range(len(holes.HOLES)):
        art = ART / "holes" / f"art_{n + 1:02d}.png"
        src = art if art.exists() else ART / "holes" / f"guide_{n + 1:02d}.png"
        lines, size = pack_page(page_small(src), f"kPage{n}")
        L += lines
        pages.append(n)
        total += size
        print(f"hole {n + 1}: {src.name} -> {size} bytes")
    # Animals.
    animal_dims = []
    for kind in holes.ANIMALS:
        path = ART / "sprites" / f"animal_{kind}.png"
        h = ANIMAL_H.get(kind, 22)
        if path.exists():
            rgb, a = sprite_from(path, height=h)
        else:
            tmp = ART / "sprites" / "_placeholder.png"
            tmp.parent.mkdir(exist_ok=True)
            placeholder_sprite(kind, h).save(tmp)
            rgb, a = sprite_from(tmp, height=h)
        lines, dims, size = pack_sprite(f"kAnimal_{kind}", rgb, a)
        L += lines
        animal_dims.append((kind, dims))
        total += size
    # Fonts.
    for name, path, size, axes in FONTS:
        lines, sz = pack_font(name, path, size, axes)
        L += lines
        total += sz
        print(f"{name}: {sz} bytes")
    L += ["", "}  // namespace", "", "const gfx::Packed kHoleArt[] = {"]
    for n in pages:
        L.append(f"    {{{W}, {H}, kPage{n}Pal, kPage{n}Data, sizeof(kPage{n}Data)}},")
    L += ["};", "", "const gfx::Image kAnimalArt[] = {"]
    for kind, (w, h) in animal_dims:
        L.append(f"    {{{w}, {h}, kAnimal_{kind}Px, kAnimal_{kind}A}},")
    L += ["};", "", "}  // namespace game", ""]
    (SRC / "assets_data.cpp").write_text("\n".join(L), encoding="utf-8")

    hdr = ["// Generated by art/game/tools/pack.py. Do not edit.", "#pragma once", "", '#include "gfx.h"', "",
           "namespace game {", "", "extern const gfx::Packed kHoleArt[];   // one per hole",
           "extern const gfx::Image kAnimalArt[];   // by animal kind:"]
    hdr += [f"//   {i} {k}" for i, k in enumerate(holes.ANIMALS)]
    hdr += [f"extern const gfx::Font {n};" for n, *_ in FONTS]
    hdr += ["", "}  // namespace game", ""]
    (SRC / "assets.h").write_text("\n".join(hdr), encoding="utf-8")
    print(f"total art: {total / 1024:.1f} KB")


if __name__ == "__main__":
    main()
