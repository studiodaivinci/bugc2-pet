"""Checks the firmware's face library against the review page, frame by frame.

Renders the same scene@time cells with the PC harness (firmware/lib/face/pc/out/sim.exe) and with the review page's
own code in headless Chrome (art/face/dump.html), then reports how far apart each cell is and writes an image with
three bands per scene: JS, C++ and the difference (x4).

    python art/face/tools/compare.py idle,mood-happy [--n 6] [--out <dir>]
    python art/face/tools/compare.py all --n 4

Build the harness first: bash firmware/lib/face/pc/build.sh
"""
import argparse
import pathlib
import socket
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
FACE = HERE.parent
ROOT = FACE.parent.parent
SIM = ROOT / "firmware" / "lib" / "face" / "pc" / "out" / "sim.exe"
W, H = 135, 240


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ids")
    ap.add_argument("--n", type=int, default=6)
    ap.add_argument("--times", default=None, help="comma list of times instead of --n")
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    out = pathlib.Path(a.out or tempfile.mkdtemp(prefix="facecmp-"))
    out.mkdir(parents=True, exist_ok=True)

    listing = subprocess.run([str(SIM), "list"], capture_output=True, text=True, check=True).stdout.splitlines()
    durs = {ln.split("\t")[0]: float(ln.split("\t")[3]) for ln in listing}
    ids = list(durs) if a.ids == "all" else a.ids.split(",")
    cells = []
    for i in ids:
        d = durs[i] or 4
        ts = [float(x) for x in a.times.split(",")] if a.times else [d * (k + 0.5) / a.n for k in range(a.n)]
        cells += [f"{i}@{t:.3f}" for t in ts]
    cols = len(cells) // len(ids)
    spec = ",".join(cells)

    cpp_path = out / "cpp.ppm"
    subprocess.run([str(SIM), "grid", spec, str(cols), str(cpp_path)], check=True, stdout=subprocess.DEVNULL)

    port = free_port()
    server = subprocess.Popen([sys.executable, "-m", "http.server", str(port), "--bind", "127.0.0.1"], cwd=FACE,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        js_path = out / "js.png"
        rows = len(ids)
        url = f"http://127.0.0.1:{port}/dump.html?cols={cols}&cells={spec}"
        r = subprocess.run([sys.executable, str(HERE / "shot.py"), url, str(js_path), "--w", str(cols * W), "--h",
                            str(rows * H), "--wait", "60"], capture_output=True, text=True)
        for line in r.stdout.splitlines():
            if not line.startswith("saved"):
                print("chrome:", line)
    finally:
        server.terminate()

    js = np.asarray(Image.open(js_path).convert("RGB")).astype(np.int16)
    cpp = np.asarray(Image.open(cpp_path).convert("RGB")).astype(np.int16)
    diff = np.abs(js - cpp).max(axis=2)
    bands = []
    print(f"{'scene':<18} " + " ".join(f"{c.split('@')[1]:>7}" for c in cells[:cols]))
    for r, sid in enumerate(ids):
        row = []
        for c in range(cols):
            d = diff[r * H:(r + 1) * H, c * W:(c + 1) * W]
            row.append(f"{d.mean():5.1f}/{(d > 40).mean() * 100:2.0f}%")
        print(f"{sid:<18} " + " ".join(f"{x:>7}" for x in row))
        sl = slice(r * H, (r + 1) * H)
        heat = np.clip(diff[sl] * 4, 0, 255).astype(np.uint8)
        bands += [js[sl].astype(np.uint8), cpp[sl].astype(np.uint8), np.stack([heat] * 3, axis=2)]
    Image.fromarray(np.concatenate(bands, axis=0)).save(out / "compare.png")
    print(f"mean abs diff / pixels off by >40 per cell. Image: {out / 'compare.png'}")


if __name__ == "__main__":
    main()
