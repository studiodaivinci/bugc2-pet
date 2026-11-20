"""Send bench commands to the robot over USB or Wi-Fi.

Usage:
  python tools/bench.py "scan" "bat" "mic 3"   USB if plugged in, otherwise Wi-Fi
  python tools/bench.py --wifi "demo"          Wi-Fi even if USB is plugged in
  python tools/bench.py --update               Build the firmware and install it over Wi-Fi

Mic captures are saved as WAV files in bench-output/.
Run tools/wifi_setup.py once over USB to put the robot on Wi-Fi.
"""

import hashlib
import json
import os
import pathlib
import shutil
import socket
import subprocess
import sys
import time
import wave

import serial
from serial.tools import list_ports

def _find_port():
    """The Stick's USB serial: BUGC2_PORT if set, else the first WCH CH9102 (USB vendor 0x1A86), else None."""
    if os.environ.get("BUGC2_PORT"):
        return os.environ["BUGC2_PORT"]
    return next((p.device for p in list_ports.comports() if p.vid == 0x1A86), None)


PORT = _find_port()
BAUD = 921600
WIFI_PORT = 3333
HOSTNAME = "bugc2-pet.local"
ROOT = pathlib.Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "bench-output"
SECRETS = ROOT / "tools" / "robot.local.json"  # Robot address and token. Never commit or print.
FIRMWARE_DIR = ROOT / "firmware" / "bench"
FIRMWARE_BIN = FIRMWARE_DIR / ".pio" / "build" / "stickc-plus2" / "firmware.bin"
PIO = shutil.which("pio") or shutil.which("platformio") or "pio"  # PlatformIO on PATH (pip install -r requirements.txt)


class WifiLink:
    """The calls bench uses from a serial port, over the robot's Wi-Fi shell."""

    def __init__(self, host, token):
        self.sock = socket.create_connection((host, WIFI_PORT), timeout=3)
        self.sock.settimeout(0.2)
        self.pending = b""
        self.write(f"auth {token}\n".encode())
        deadline = time.time() + 3
        while time.time() < deadline:
            if self.readline().strip() == b"OK":
                return
        raise ConnectionError("the robot did not accept the token")

    def write(self, data):
        self.sock.settimeout(60)
        self.sock.sendall(data)
        self.sock.settimeout(0.2)

    def _fill(self):
        try:
            chunk = self.sock.recv(65536)
        except TimeoutError:
            return
        if not chunk:
            raise ConnectionError("the robot closed the connection")
        self.pending += chunk

    def read(self, size):
        if not self.pending:
            self._fill()
        data, self.pending = self.pending[:size], self.pending[size:]
        return data

    def readline(self):
        if b"\n" not in self.pending:
            self._fill()
        if b"\n" not in self.pending:
            return b""
        line, self.pending = self.pending.split(b"\n", 1)
        return line + b"\n"

    def reset_input_buffer(self):
        self.pending = b""

    def close(self):
        self.sock.close()


def connect_usb():
    # DTR and RTS are wired to reset, so keep them released to avoid rebooting the robot.
    port = serial.Serial()
    port.port = PORT
    port.baudrate = BAUD
    port.timeout = 0.2
    port.dtr = False
    port.rts = False
    port.open()
    return port


def connect_wifi():
    if not SECRETS.exists():
        raise SystemExit("No Wi-Fi details yet. Run tools/wifi_setup.py over USB first.")
    config = json.loads(SECRETS.read_text())
    for host in (config["host"], HOSTNAME):
        try:
            return WifiLink(host, config["token"])
        except OSError:
            continue
    raise SystemExit("Can't reach the robot over Wi-Fi. Is it switched on?")


def connect(wifi=False):
    if not wifi and PORT in [item.device for item in list_ports.comports()]:
        return connect_usb()
    return connect_wifi()


def save_audio(data):
    OUTPUT.mkdir(exist_ok=True)
    path = OUTPUT / time.strftime("mic-%H%M%S.wav")
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(16000)
        wav.writeframes(data)
    print(f"saved {path}")


def run(port, command, timeout=90):
    port.reset_input_buffer()
    port.write((command + "\n").encode())
    print(f"> {command}")
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
            print(f"received {len(data)} of {length} audio bytes")
            save_audio(data)
            continue
        if line == "OK":
            return True
        print(line)
        if line.startswith("ERR"):
            return False
    print("TIMEOUT")
    return False


def update_firmware():
    subprocess.run([str(PIO), "run", "-d", str(FIRMWARE_DIR)], check=True, stdout=subprocess.DEVNULL)
    image = FIRMWARE_BIN.read_bytes()
    md5 = hashlib.md5(image).hexdigest()
    link = connect_wifi()
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
        print(f"sent {len(image)} bytes in {time.time() - started:.0f} s")
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


def main():
    args = sys.argv[1:]
    if "--update" in args:
        update_firmware()
        return
    wifi = "--wifi" in args
    port = connect(wifi)
    try:
        for command in args:
            if command != "--wifi":
                run(port, command)
    finally:
        port.close()


if __name__ == "__main__":
    main()
