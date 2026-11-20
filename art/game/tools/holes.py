"""The holes, in one place: geometry for the physics, the colour-coded guide that local AI repaints into the paper
page, and the C++ data the game runs on.

    python holes.py            # writes art/game/holes/guide_NN.png and firmware/lib/game/src/course_data.cpp
    python holes.py --overlay  # also draws each hole's rails and zones over its picked art (art/game/holes/check_NN.png)

Coordinates are screen pixels on the 135x240 portrait screen; y grows down. The top 24 px stay clear of the course
for the HUD tabs. Rails run along the edges of `green` and of each `island`; the drawn rail is 6 px wide, centred on
the edge, and the physics keeps the ball's centre ballR + 3 px from the edge.
"""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUT_DIR = HERE.parent / "holes"
CPP = ROOT / "firmware" / "lib" / "game" / "src" / "course_data.cpp"

W, H = 135, 240
S = 16 / 3  # guide pixels per screen pixel (720x1280)

# Surface codes (course.h)
FELT, SAND, WATER, ICE, SLOPE0, OUT = 0, 1, 2, 3, 4, 12

# Animal art, in the order of the sprite table (tools/pack.py reads this list too).
ANIMALS = ["tortoise", "rabbit", "hedgehog", "frog", "duck", "beaver", "penguin", "seal", "polarbear"]
HABITATS = [("Meadow", (230, 64, 52)), ("Pond", (236, 110, 160)), ("Arctic", (200, 40, 70))]

# Guide colours: what each colour becomes is spelled out in the habitat's repaint prompt.
G_OUTSIDE = (38, 104, 48)
G_FELT = (96, 196, 84)
G_RAIL = (244, 236, 214)
G_SAND = (236, 204, 132)
G_WATER = (58, 132, 222)
G_ICE = (196, 230, 246)
G_HILL = [(120, 210, 100), (146, 222, 120), (172, 234, 142)]  # contour layers, outer to inner
G_BUMPER = (140, 140, 140)
G_CUP = (20, 20, 20)
G_FLAG = (230, 40, 40)
G_TEE = (250, 250, 250)


def ellipse(cx, cy, rx, ry):
    return ("ellipse", cx, cy, rx, ry)


def poly(*pts):
    return ("poly", list(pts))


# --- The holes ----------------------------------------------------------------------------------------------------
# zones: (kind, shape) painted in order; kind is "sand", "water", "ice", "felt" (e.g. a bridge) or ("hill", +1/-1)
# for a raised hill (pushes out) or a dip (pulls in), drawn as paper contour layers.
# animals: (kind, motion, radius, speed, [points]); motion is walk, hop, pop or sleep.

HOLES = [
    dict(
        name="Tortoise Trot", par=2, habitat=0, unlock=0,
        green=[(30, 234), (105, 234), (115, 224), (115, 46), (105, 34), (30, 34), (20, 46), (20, 224)],
        tee=(67, 216), cup=(67, 58),
        zones=[("sand", ellipse(36, 96, 11, 13)), ("sand", ellipse(99, 172, 11, 13))],
        animals=[("tortoise", "walk", 9, 9, [(38, 132), (96, 132)])],
        fact="Giant tortoises\ncan live for more\nthan 150 years!",
    ),
    dict(
        name="Bunny Bend", par=3, habitat=0, unlock=0,
        green=[(72, 234), (116, 234), (116, 42), (106, 32), (28, 32), (18, 42), (18, 84), (28, 94), (64, 94),
               (72, 104)],
        tee=(94, 216), cup=(36, 58),
        zones=[("sand", ellipse(100, 58, 10, 13)), (("hill", 1), ellipse(94, 160, 15, 15))],
        animals=[("rabbit", "hop", 8, 5, [(56, 64), (86, 74), (94, 112)])],
        fact="A rabbit's teeth\nnever stop\ngrowing!",
    ),
    dict(
        name="Hedgehog Hill", par=3, habitat=0, unlock=0,
        green=[(26, 234), (110, 234), (120, 222), (120, 132), (112, 112), (112, 44), (102, 32), (34, 32),
               (24, 44), (24, 112), (16, 132), (16, 222)],
        tee=(68, 218), cup=(68, 50),
        zones=[("water", ellipse(33, 180, 9, 17)), ("sand", ellipse(98, 70, 9, 10)),
               (("hill", 1), ellipse(68, 104, 19, 17))],
        animals=[("hedgehog", "walk", 8, 12, [(40, 150), (98, 150)])],
        fact="A hedgehog has\nabout 6,000\nspines!",
    ),
]

# --- Geometry helpers ---------------------------------------------------------------------------------------------


def shape_contains(shape, x, y):
    if shape[0] == "ellipse":
        _, cx, cy, rx, ry = shape
        return ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1
    return point_in_poly(shape[1], x, y)


def point_in_poly(pts, x, y):
    inside = False
    j = len(pts) - 1
    for i in range(len(pts)):
        xi, yi = pts[i]
        xj, yj = pts[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def surface_map(h):
    grid = bytearray([OUT]) * (W * H)
    for y in range(H):
        for x in range(W):
            px, py = x + 0.5, y + 0.5
            if not point_in_poly(h["green"], px, py):
                continue
            if any(point_in_poly(isl, px, py) for isl in h.get("islands", [])):
                continue
            code = FELT
            for kind, shape in h.get("zones", []):
                if not shape_contains(shape, px, py):
                    continue
                if kind == "sand":
                    code = SAND
                elif kind == "water":
                    code = WATER
                elif kind == "ice":
                    code = ICE
                elif kind == "felt":
                    code = FELT
                elif isinstance(kind, tuple) and kind[0] == "hill":
                    cx, cy = shape[1], shape[2]
                    ang = math.degrees(math.atan2(-(py - cy), px - cx))  # screen up = +90
                    if kind[1] < 0:
                        ang += 180
                    code = SLOPE0 + int(round(ang / 45)) % 8
            grid[y * W + x] = code
    return grid


def rle(grid):
    out = bytearray()
    i = 0
    while i < len(grid):
        v, n = grid[i], 1
        while i + n < len(grid) and grid[i + n] == v and n < 256:
            n += 1
        out += bytes([n - 1, v])
        i += n
    return out


def segments(h):
    segs = []
    for loop in [h["green"]] + h.get("islands", []):
        for i in range(len(loop)):
            a, b = loop[i], loop[(i + 1) % len(loop)]
            segs.append((a[0], a[1], b[0], b[1]))
    return segs


# --- Guide image --------------------------------------------------------------------------------------------------


def sc(pts):
    return [(x * S, y * S) for x, y in pts]


def draw_shape(d, shape, fill, grow=0):
    if shape[0] == "ellipse":
        _, cx, cy, rx, ry = shape
        d.ellipse([(cx - rx - grow) * S, (cy - ry - grow) * S, (cx + rx + grow) * S, (cy + ry + grow) * S], fill=fill)
    else:
        d.polygon(sc(shape[1]), fill=fill)


def guide(h):
    im = Image.new("RGB", (int(W * S), int(H * S)), G_OUTSIDE)
    d = ImageDraw.Draw(im)
    d.polygon(sc(h["green"]), fill=G_FELT)
    for kind, shape in h.get("zones", []):
        if isinstance(kind, tuple) and kind[0] == "hill":
            # Paper contour layers: three rings stepping in.
            for k, col in enumerate(G_HILL):
                f = 1 - k * 0.3
                if shape[0] == "ellipse":
                    _, cx, cy, rx, ry = shape
                    draw_shape(d, ellipse(cx, cy, rx * f, ry * f), col)
        else:
            col = {"sand": G_SAND, "water": G_WATER, "ice": G_ICE, "felt": G_FELT}[kind]
            draw_shape(d, shape, col)
    for isl in h.get("islands", []):
        d.polygon(sc(isl), fill=G_OUTSIDE)
    for loop in [h["green"]] + h.get("islands", []):
        d.line(sc(loop + loop[:1]), fill=G_RAIL, width=int(6 * S), joint="curve")
        for x, y in loop:  # round, closed corners
            d.ellipse([(x - 3) * S, (y - 3) * S, (x + 3) * S, (y + 3) * S], fill=G_RAIL)
    for bx, by, br, _ in h.get("bumpers", []):
        d.ellipse([(bx - br) * S, (by - br) * S, (bx + br) * S, (by + br) * S], fill=G_BUMPER)
    tx, ty = h["tee"]
    d.rectangle(sc([(tx - 8, ty - 5), (tx + 8, ty + 5)]), fill=G_TEE)
    cx, cy = h["cup"]
    d.ellipse(sc([(cx - 5.5, cy - 5.5), (cx + 5.5, cy + 5.5)]), fill=G_CUP)
    d.line(sc([(cx, cy), (cx, cy - 22)]), fill=(250, 250, 250), width=int(1.5 * S))
    d.polygon(sc([(cx, cy - 22), (cx + 14, cy - 17), (cx, cy - 12)]), fill=G_FLAG)
    return im


def overlay(h, art_path, out_path):
    art = Image.open(art_path).convert("RGB").resize((W, H), Image.LANCZOS).resize((W * 4, H * 4), Image.NEAREST)
    d = ImageDraw.Draw(art)
    for x1, y1, x2, y2 in segments(h):
        d.line([(x1 * 4, y1 * 4), (x2 * 4, y2 * 4)], fill=(255, 0, 255), width=2)
    for kind, shape in h.get("zones", []):
        if shape[0] == "ellipse":
            _, cx, cy, rx, ry = shape
            d.ellipse([(cx - rx) * 4, (cy - ry) * 4, (cx + rx) * 4, (cy + ry) * 4], outline=(0, 255, 255), width=2)
    cx, cy = h["cup"]
    d.ellipse([(cx - 5.5) * 4, (cy - 5.5) * 4, (cx + 5.5) * 4, (cy + 5.5) * 4], outline=(255, 255, 0), width=2)
    for _, _, r, _, pts in h.get("animals", []):
        d.line([(x * 4, y * 4) for x, y in pts], fill=(255, 160, 0), width=2)
    art.save(out_path)


# --- C++ ----------------------------------------------------------------------------------------------------------


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def write_cpp(holes):
    motion = {"walk": "Walk", "hop": "Hop", "pop": "Pop", "sleep": "Sleep"}
    L = ["// Generated by art/game/tools/holes.py. Do not edit; change the holes there and rerun it.",
         '#include "course.h"', '#include "assets.h"', "", "namespace game {", "", "namespace {", ""]
    for n, h in enumerate(holes):
        segs = segments(h)
        L.append(f"const Seg kSegs{n}[] = {{")
        L += [f"    {{{a}, {b}, {c}, {d}}}," for a, b, c, d in segs]
        L.append("};")
        if h.get("bumpers"):
            L.append(f"const Bumper kBumpers{n}[] = {{")
            L += [f"    {{{x}, {y}, {r}, {b}}}," for x, y, r, b in h["bumpers"]]
            L.append("};")
        for k, (kind, mot, r, sp, pts) in enumerate(h.get("animals", [])):
            flat = ", ".join(f"{x}, {y}" for x, y in pts)
            L.append(f"const int16_t kPath{n}_{k}[] = {{{flat}}};")
        if h.get("animals"):
            L.append(f"const AnimalDef kAnimals{n}[] = {{")
            for k, (kind, mot, r, sp, pts) in enumerate(h["animals"]):
                L.append(f"    {{{ANIMALS.index(kind)}, Motion::{motion[mot]}, {r}, {sp}, {len(pts)}, kPath{n}_{k}}},")
            L.append("};")
        data = rle(surface_map(h))
        L.append(f"const uint8_t kSurface{n}[] = {{")
        for i in range(0, len(data), 24):
            L.append("    " + ", ".join(str(b) for b in data[i:i + 24]) + ",")
        L.append("};")
        L.append("")
    L += ["}  // namespace", "", "const Hole kHoles[] = {"]
    for n, h in enumerate(holes):
        tx, ty = h["tee"]
        cx, cy = h["cup"]
        nb = len(h.get("bumpers", []))
        na = len(h.get("animals", []))
        L.append(f"    {{{cstr(h['name'])}, {h['par']}, {h['habitat']}, {h['unlock']}, {tx}, {ty}, {cx}, {cy}, "
                 f"kSegs{n}, {len(segments(h))}, {f'kBumpers{n}' if nb else 'nullptr'}, {nb}, "
                 f"{f'kAnimals{n}' if na else 'nullptr'}, {na}, kSurface{n}, sizeof(kSurface{n}), "
                 f"&kHoleArt[{n}], {cstr(h['fact'])}}},")
    L += ["};", f"const uint8_t kHoleCount = {len(holes)};", "", "const Habitat kHabitats[] = {"]
    for name, (r, g, b) in HABITATS:
        L.append(f"    {{{cstr(name)}, gfx::rgb({r}, {g}, {b})}},")
    L += ["};", f"const uint8_t kHabitatCount = {len(HABITATS)};", "", "}  // namespace game", ""]
    CPP.write_text("\n".join(L), encoding="utf-8")


# --- Repaint prompts (the prompt spine in art/game/design/DESIGN.md) ------------------------------------------------

HABITAT_WORDS = {
    0: dict(scene="in a spring meadow",
            outside="layers of deep green paper cut into leafy bushes, grass tufts, yellow buttercups and red poppies",
            course="a sheet of bright spring-green card"),
    1: dict(scene="beside a pond",
            outside="layers of teal and blue paper water with cut-paper lily pads, reeds and pink water lilies",
            course="a sheet of bright spring-green card"),
    2: dict(scene="on the Arctic ice",
            outside="layers of deep blue paper sea with white cut-paper ice floes and paper snowflakes",
            course="a sheet of pale icy-blue card"),
}


def prompt(h):
    w = HABITAT_WORDS[h["habitat"]]
    kinds = {k if isinstance(k, str) else k[0] for k, _ in h.get("zones", [])}
    parts = [f"the light green area becomes {w['course']}",
             "the cream border becomes raised cream card strips standing on edge",
             f"the dark green outside becomes {w['outside']}"]
    if "sand" in kinds:
        parts.append("the tan ovals become sand traps of yellow paper with punched dots")
    if "water" in kinds:
        parts.append("the blue shapes become ponds of layered blue paper ripples")
    if "ice" in kinds:
        parts.append("the very pale blue areas become slippery glossy ice made of pale blue foil paper")
    if "hill" in kinds:
        parts.append("the pale green rings become a small round hill built from stacked contour layers of lighter "
                     "green card, each layer a little smaller")
    if h.get("bumpers"):
        parts.append("the grey circles become round paper boulders")
    parts.append("the black circle becomes the cup with a red paper flag on a white pin")
    parts.append("the white rectangle becomes a white paper tee mat")
    return ("Repaint this color-coded layout map as an overhead photograph looking straight down at a layered "
            f"cut-paper pop-up book page showing a mini golf hole {w['scene']}. Keep the exact layout and position "
            "of every shape. " + "; ".join(parts)[0].upper() + "; ".join(parts)[1:] + ". Each layer casts a soft shadow on the one below, soft light "
            "from the top left, crisp clean edges, bright colours, no animals, no text, no people.")


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    prompts = ["# Generated by holes.py: one repaint prompt per hole, each with its own layout guide."]
    for n, h in enumerate(HOLES):
        guide(h).save(OUT_DIR / f"guide_{n + 1:02d}.png")
        prompts += ["", f"# hole {n + 1}: {h['name']}", f"@ref guide_{n + 1:02d}.png", prompt(h)]
    (OUT_DIR / "prompts.txt").write_text("\n".join(prompts) + "\n", encoding="utf-8")
    for n, h in enumerate(HOLES):
        art = OUT_DIR / f"art_{n + 1:02d}.png"
        if "--overlay" in sys.argv and art.exists():
            overlay(h, art, OUT_DIR / f"check_{n + 1:02d}.png")
    write_cpp(HOLES)
    print(f"{len(HOLES)} holes -> {OUT_DIR} and {CPP}")


if __name__ == "__main__":
    main()
