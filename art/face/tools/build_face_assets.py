"""Builds the face library's data file (firmware/lib/face/src/face_assets_data.cpp) from art/face.

- Fonts: Bagel Fat One (shouts) and Lilita One (sentences) as signed-distance glyphs, so the robot draws captions at
  any size, scaled and rotated, with an outline, all from one table per font. Printable ASCII plus the degree sign.
- Toy art: assets/*.png at the sizes the review page loads them (longest side 100 px; egg 112, cake 104), straight
  RGBA, resized with premultiplied Lanczos.

    python art/face/tools/build_face_assets.py
"""
import pathlib

import numpy as np
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont
from scipy.ndimage import distance_transform_edt

HERE = pathlib.Path(__file__).resolve().parent
FACE = HERE.parent
OUT = FACE.parent.parent / "firmware" / "lib" / "face" / "src" / "face_assets_data.cpp"

EM = 32       # glyph table em size in px
SPREAD = 8.0  # distance range stored either side of the edge, in em-32 px
SS = 8        # supersampling for the distance transform
PAD = 8       # empty cells around each glyph (>= SPREAD so outlines are never cut)
CHARS = [chr(c) for c in range(32, 127)] + ["°"]

FONTS = [("SHOUT", "BagelFatOne-Regular.ttf"), ("SAY", "LilitaOne-Regular.ttf")]
ASSETS = ["cheetah", "honeybee", "hummingbird", "spider", "owl", "dolphin", "bat", "chameleon", "egg", "cake",
          "treat", "flower", "golfflag", "whirligig", "sound", "remote"]
SIZES = {"egg": 112, "cake": 104}


def kerning_pairs(path, chars):
    """Pair kerning from GPOS (PairPos formats 1 and 2), in font units, for the characters we keep."""
    font = TTFont(path)
    cmap = font.getBestCmap()
    glyph_of = {c: cmap.get(ord(c)) for c in chars}
    want = {g: c for c, g in glyph_of.items() if g}
    pairs = {}
    if "GPOS" not in font:
        return pairs, font["head"].unitsPerEm
    gpos = font["GPOS"].table
    for lookup in gpos.LookupList.Lookup:
        subs = lookup.SubTable
        if lookup.LookupType == 9:
            subs = [s.ExtSubTable for s in subs]
        elif lookup.LookupType != 2:
            continue
        for st in subs:
            if getattr(st, "LookupType", 2) != 2:
                continue
            cov = st.Coverage.glyphs
            if st.Format == 1:
                for i, first in enumerate(cov):
                    if first not in want:
                        continue
                    for pvr in st.PairSet[i].PairValueRecord:
                        if pvr.SecondGlyph in want and pvr.Value1 is not None:
                            v = getattr(pvr.Value1, "XAdvance", 0) or 0
                            if v:
                                pairs.setdefault((want[first], want[pvr.SecondGlyph]), v)
            elif st.Format == 2:
                cd1 = st.ClassDef1.classDefs if st.ClassDef1 else {}
                cd2 = st.ClassDef2.classDefs if st.ClassDef2 else {}
                for first in cov:
                    if first not in want:
                        continue
                    c1 = cd1.get(first, 0)
                    for second, cs in want.items():
                        c2 = cd2.get(second, 0)
                        rec = st.Class1Record[c1].Class2Record[c2]
                        v = getattr(rec.Value1, "XAdvance", 0) if rec.Value1 is not None else 0
                        if v:
                            pairs.setdefault((want[first], cs), v)
    return pairs, font["head"].unitsPerEm


def build_font(tag, filename, out):
    path = FACE / "fonts" / filename
    hi = ImageFont.truetype(str(path), EM * SS)
    glyphs, blob = [], bytearray()
    for ch in CHARS:
        adv = hi.getlength(ch) / SS
        l, t, r, b = hi.getbbox(ch, anchor="ls")
        if ch == " " or r <= l or b <= t:
            glyphs.append((ord(ch), 0, 0, 0, 0, adv, len(blob)))
            continue
        x0, y0 = int(np.floor(l / SS)) - PAD, int(np.floor(t / SS)) - PAD
        x1, y1 = int(np.ceil(r / SS)) + PAD, int(np.ceil(b / SS)) + PAD
        w, h = x1 - x0, y1 - y0
        im = Image.new("L", (w * SS, h * SS), 0)
        ImageDraw.Draw(im).text((-x0 * SS, -y0 * SS), ch, font=hi, fill=255, anchor="ls")
        inside = np.asarray(im) >= 128
        sd = distance_transform_edt(~inside) - distance_transform_edt(inside)  # + outside, hi-res px
        sd = sd.reshape(h, SS, w, SS).mean(axis=(1, 3)) / SS                  # em-32 px
        q = np.clip(np.round(128 - np.clip(sd / SPREAD, -1, 1) * 127), 0, 255).astype(np.uint8)
        glyphs.append((ord(ch), w, h, x0, y0, adv, len(blob)))
        blob += q.tobytes()
    pairs, upm = kerning_pairs(path, CHARS)
    kern = sorted((ord(a), ord(b), v * EM / upm) for (a, b), v in pairs.items() if abs(v * EM / upm) >= 0.05)
    out.append(f"static const uint8_t k{tag}Sdf[] = {{{carr(blob)}}};")
    out.append(f"static const Glyph k{tag}Glyphs[] = {{")
    for cp, w, h, x0, y0, adv, off in glyphs:
        out.append(f"  {{{cp}, {w}, {h}, {x0}, {y0}, {adv:.4f}f, {off}u}},")
    out.append("};")
    out.append(f"static const Kern k{tag}Kern[] = {{")
    for a, b, v in kern:
        out.append(f"  {{{a}, {b}, {v:.4f}f}},")
    if not kern:
        out.append("  {0, 0, 0.0f},")
    out.append("};")
    out.append(f"const Font FONT_{tag} = {{{EM}.0f, {SPREAD}f, k{tag}Glyphs, {len(glyphs)}, k{tag}Sdf, "
               f"k{tag}Kern, {len(kern)}}};")
    print(f"{filename}: {len(glyphs)} glyphs, {len(blob)} bytes, {len(kern)} kerning pairs")
    return len(blob)


def build_sprites(out):
    total = 0
    names = []
    for key in ASSETS:
        im = Image.open(FACE / "assets" / f"{key}.png").convert("RGBA")
        m = SIZES.get(key, 100)
        k = m / max(im.size)
        w, h = max(1, round(im.width * k)), max(1, round(im.height * k))
        im = im.convert("RGBa").resize((w, h), Image.LANCZOS).convert("RGBA")
        data = np.asarray(im).astype(np.uint8)
        data[data[..., 3] == 0] = 0
        out.append(f"static const uint8_t kImg_{key}[] = {{{carr(data.tobytes())}}};")
        names.append((key, w, h))
        total += w * h * 4
    out.append("const Bitmap IMAGES[] = {")
    for key, w, h in names:
        out.append(f"  {{{w}, {h}, kImg_{key}}},  // {key}")
    out.append("};")
    out.append("const char* const IMAGE_NAMES[] = {" + ", ".join(f'"{k}"' for k, _, _ in names) + "};")
    out.append(f"const int IMAGE_COUNT = {len(names)};")
    print(f"sprites: {len(names)} images, {total} bytes")
    return total


def carr(b):
    b = bytes(b)
    lines = []
    for i in range(0, len(b), 40):
        lines.append(",".join(str(x) for x in b[i:i + 40]))
    return "\n" + ",\n".join(lines) + "\n"


def main():
    out = ["// Generated by art/face/tools/build_face_assets.py. Do not edit.",
           "// Fonts: Bagel Fat One and Lilita One (SIL Open Font License, art/face/fonts/OFL-*.txt).",
           "// Toy art: Qwen-Image 2.1 (Qwen Research License, non-commercial; not under this repo's MIT licence), art/face/assets.",
           '#include "face_assets.h"', "", "namespace face {", ""]
    n = sum(build_font(tag, fn, out) for tag, fn in FONTS)
    n += build_sprites(out)
    out += ["", "}  // namespace face", ""]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(out), encoding="utf-8")
    print(f"wrote {OUT} ({n / 1024:.0f} KB of data)")


if __name__ == "__main__":
    main()
