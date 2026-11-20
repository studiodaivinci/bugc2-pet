"""Installs the game test firmware on the robot over Wi-Fi.

Uses the robot's own update protocol (the same one `python tools/bench.py --update` uses), so it works while the
robot runs either the bench firmware or this one. Reads the address and token from tools/robot.local.json through
bench.py and never prints them. Builds first.

    python firmware/game-test/upload_wifi.py

To put the robot firmware back afterwards: python tools/bench.py --update
"""
import hashlib
import pathlib
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import bench  # noqa: E402  (read only: we use its Wi-Fi connection)

FIRMWARE_BIN = HERE / ".pio" / "build" / "stickc-plus2" / "firmware.bin"


def main():
    subprocess.run([str(bench.PIO), "run", "-d", str(HERE)], check=True, stdout=subprocess.DEVNULL)
    image = FIRMWARE_BIN.read_bytes()
    md5 = hashlib.md5(image).hexdigest()
    print(f"game-test firmware: {len(image)} bytes")
    link = bench.connect_wifi()
    try:
        link.write(f"update {len(image)} {md5}\n".encode())
        deadline = time.time() + 15
        while time.time() < deadline:
            line = link.readline().decode(errors="replace").strip()
            if line == "SEND":
                break
            if line.startswith("ERR"):
                raise SystemExit(line)
        else:
            raise SystemExit("The robot did not start the update.")
        started = time.time()
        link.write(image)
        print(f"sent in {time.time() - started:.0f} s")
        deadline = time.time() + 30
        while time.time() < deadline:
            line = link.readline().decode(errors="replace").strip()
            if line:
                print(line)
            if line == "OK":
                return
            if line.startswith("ERR"):
                raise SystemExit(1)
        raise SystemExit("No reply after sending the update.")
    finally:
        link.close()


if __name__ == "__main__":
    main()
