"""Bench captures: gestures, voice samples, the clock and the battery log.

Usage:
  python tools/capture.py gesture <name> [seconds]     Motion log at 200 Hz, saved as CSV
  python tools/capture.py voice <speaker> <condition>  Each word 5 times, saved as WAV files
  python tools/capture.py clockset                     Sets the robot's clock to this PC's time
  python tools/capture.py clock                        Robot time next to this PC's time
  python tools/capture.py batlog                       The battery run-down log

The robot's screen shows the gesture or word, so whoever is holding it doesn't need this window.
Files go in bench-output/gestures/ and bench-output/voice/<speaker>/<condition>/.
"""

import csv
import math
import sys
import time
import wave

import bench

WORDS = ["CHEETAH", "HONEYBEE", "HUMMINGBIRD", "SPIDER", "OWL", "DOLPHIN"]
REPS = 5
CLIP_SECONDS = 2.0


def run_collect(port, command, timeout=90):
    """Sends a command and returns (ok, lines, audio bytes or None)."""
    port.reset_input_buffer()
    port.write((command + "\n").encode())
    lines, audio = [], None
    deadline = time.time() + timeout
    while time.time() < deadline:
        raw = port.readline()
        if not raw:
            continue
        line = raw.decode(errors="replace").strip()
        if line.startswith("AUDIO "):
            length = int(line.split()[1])
            data = b""
            while len(data) < length and time.time() < deadline:
                data += port.read(length - len(data))
            audio = data
            continue
        if line == "OK":
            return True, lines, audio
        lines.append(line)
        if line.startswith("ERR"):
            return False, lines, audio
    return False, lines + ["TIMEOUT"], audio


def gesture(name, seconds):
    port = bench.connect()
    try:
        ok, lines, _ = run_collect(port, f"imulog {int(seconds * 1000)} {name.upper()}")
    finally:
        port.close()
    start = next((i for i, l in enumerate(lines) if l.startswith("IMULOG")), None)
    if not ok or start is None:
        raise SystemExit("\n".join(lines[-5:]))
    rows = []
    for line in lines[start + 1:]:
        if line == "IMUEND":
            break
        rows.append([float(x) for x in line.split()])
    folder = bench.OUTPUT / "gestures"
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / f"{name.lower()}-{time.strftime('%Y%m%d-%H%M%S')}.csv"
    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["ms", "ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps"])
        writer.writerows(rows)
    duration = rows[-1][0] / 1000 if rows else 0
    peak_g = max((math.sqrt(r[1] ** 2 + r[2] ** 2 + r[3] ** 2) for r in rows), default=0)
    peak_spin = max((math.sqrt(r[4] ** 2 + r[5] ** 2 + r[6] ** 2) for r in rows), default=0)
    print(f"saved {path}")
    print(f"{len(rows)} samples over {duration:.1f} s ({len(rows) / max(duration, 0.001):.0f} Hz), "
          f"peak {peak_g:.2f} g, peak turn rate {peak_spin:.0f} deg/s")


def voice(speaker, condition):
    folder = bench.OUTPUT / "voice" / speaker.lower() / condition.lower()
    folder.mkdir(parents=True, exist_ok=True)
    port = bench.connect()
    saved = 0
    try:
        for rep in range(1, REPS + 1):  # Round robin, so tiredness spreads across the words.
            for word in WORDS:
                ok, lines, audio = run_collect(port, f"mic {CLIP_SECONDS} {word}")
                if not ok or not audio:
                    raise SystemExit(f"{word} {rep}: " + " | ".join(lines[-3:]))
                path = folder / f"{word.lower()}-{rep}.wav"
                with wave.open(str(path), "wb") as wav:
                    wav.setnchannels(1)
                    wav.setsampwidth(2)
                    wav.setframerate(16000)
                    wav.writeframes(audio)
                saved += 1
                print(f"{word.lower()}-{rep} ok", flush=True)
    finally:
        port.close()
    print(f"saved {saved} clips in {folder}")


def simple(command):
    port = bench.connect()
    try:
        ok, lines, _ = run_collect(port, command)
    finally:
        port.close()
    print("\n".join(lines))
    return ok


def main():
    args = sys.argv[1:]
    if not args:
        raise SystemExit(__doc__)
    if args[0] == "gesture" and len(args) >= 2:
        gesture(args[1], float(args[2]) if len(args) > 2 else 15)
    elif args[0] == "voice" and len(args) == 3:
        voice(args[1], args[2])
    elif args[0] == "clockset":
        # Connect first, then send the next whole second just before it starts.
        port = bench.connect()
        try:
            target = math.floor(time.time()) + 1
            time.sleep(max(0.0, target - time.time() - 0.05))
            t = time.localtime(target)
            _, lines, _ = run_collect(
                port, f"clock set {t.tm_year} {t.tm_mon} {t.tm_mday} {t.tm_hour} {t.tm_min} {t.tm_sec}")
        finally:
            port.close()
        print("\n".join(lines))
    elif args[0] == "clock":
        simple("clock")
        print("pc    " + time.strftime("%Y-%m-%d %H:%M:%S"))
    elif args[0] == "batlog":
        simple("batlog")
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
