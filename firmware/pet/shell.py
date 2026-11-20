"""Sends commands to the pet firmware over Wi-Fi (dev build) and prints what it says back, each line timestamped.

    python firmware/pet/shell.py info
    python firmware/pet/shell.py trace --listen 600 --out bench-output/handling/run1.log

Each argument is one command ("wait:N" pauses N seconds); --listen is how long to keep printing afterwards; --out also
writes the lines to a file. Uses bench.py's Wi-Fi connection (address and token from tools/robot.local.json, never
printed). "trace" toggles, and stays on after this exits: send it again to stop it.
"""
import argparse
import pathlib
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent / "tools"))
import bench  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("commands", nargs="+")
    ap.add_argument("--listen", type=float, default=2)
    ap.add_argument("--out")
    a = ap.parse_args()
    out = None
    if a.out:
        pathlib.Path(a.out).parent.mkdir(parents=True, exist_ok=True)
        out = open(a.out, "a", encoding="utf-8")
    start = time.time()

    def show(line):
        stamped = f"{time.time() - start:7.2f} {line}"
        print(stamped, flush=True)
        if out:
            out.write(stamped + "\n")
            out.flush()

    link = bench.connect_wifi()
    try:
        for cmd in a.commands:
            if cmd.startswith("wait:"):
                time.sleep(float(cmd[5:]))
                continue
            link.write((cmd + "\n").encode())
            time.sleep(0.2)
        end = time.time() + a.listen
        while time.time() < end:
            raw = link.readline()
            if raw:
                show(raw.decode(errors="replace").rstrip())
    finally:
        link.close()
        if out:
            out.close()


if __name__ == "__main__":
    main()
