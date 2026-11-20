"""Sends commands to the face test firmware over Wi-Fi and prints what it says back.

    python firmware/face-test/shell.py "play birthday" [--listen 5]
    python firmware/face-test/shell.py bench --listen 420

Each argument is one command; --listen is how long to keep printing afterwards (seconds). Uses bench.py's Wi-Fi
connection (address and token from tools/robot.local.json, never printed). Run with a Python that has pyserial.
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
    a = ap.parse_args()
    link = bench.connect_wifi()
    try:
        for cmd in a.commands:
            if cmd.startswith("wait:"):  # wait:N pauses N seconds between commands
                time.sleep(float(cmd[5:]))
                continue
            link.write((cmd + "\n").encode())
            time.sleep(0.2)
        end = time.time() + a.listen
        while time.time() < end:
            raw = link.readline()
            if raw:
                print(raw.decode(errors="replace").rstrip(), flush=True)
    finally:
        link.close()


if __name__ == "__main__":
    main()
