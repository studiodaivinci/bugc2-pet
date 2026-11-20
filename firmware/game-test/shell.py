"""Talks to the game test firmware over Wi-Fi: runs commands and saves screenshots.

    python firmware/game-test/shell.py info
    python firmware/game-test/shell.py shot            # saves art/game/shots/shot-HHMMSS.png
    python firmware/game-test/shell.py watch 60        # prints game events and button presses for 60 s

Uses tools/bench.py's connection (address and token from tools/robot.local.json, never printed).
"""
import pathlib
import struct
import sys
import time
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import bench  # noqa: E402

SHOTS = ROOT / "art" / "game" / "shots"


def connect(wait=20):
    deadline = time.time() + wait
    while True:
        try:
            return bench.connect_wifi()
        except SystemExit:
            if time.time() > deadline:
                raise
            time.sleep(2)


def lines(link, seconds):
    end = time.time() + seconds
    while time.time() < end:
        line = link.readline()
        if line:
            print(line.decode(errors="replace").rstrip())


def shot(link):
    link.write(b"shot\n")
    end = time.time() + 10
    while time.time() < end:
        line = link.readline().decode(errors="replace").strip()
        if line.startswith("SHOT "):
            break
    else:
        raise SystemExit("no screenshot")
    size = int(line.split()[1])
    data = b""
    while len(data) < size and time.time() < end:
        data += link.read(size - len(data))
    rows = []
    for y in range(240):
        row = bytearray(b"\0")  # PNG filter: none
        for x in range(135):
            i = (y * 135 + x) * 2
            v = (data[i] << 8) | data[i + 1]  # stored byte-swapped
            rgb = bytes((((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31))
            row += rgb * 2  # twice the size, so it's easier to read
        rows += [bytes(row), bytes(row)]
    SHOTS.mkdir(parents=True, exist_ok=True)
    path = SHOTS / time.strftime("shot-%H%M%S.png")
    path.write_bytes(png(270, 480, b"".join(rows)))
    print(f"saved {path}")


def png(w, h, raw):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    args = sys.argv[1:] or ["info"]
    link = connect()
    try:
        if args[0] == "shot":
            shot(link)
        elif args[0] == "watch":
            link.write(b"keys\n")
            lines(link, float(args[1]) if len(args) > 1 else 60)
            link.write(b"keys\n")
            lines(link, 0.5)
        else:
            link.write((" ".join(args) + "\n").encode())
            lines(link, 2)
    finally:
        link.close()


if __name__ == "__main__":
    main()
