"""Put the robot on Wi-Fi. Run once with the Stick plugged in over USB.

A small window asks for the Wi-Fi name and password. The password goes straight to the robot over
USB, where it is stored. It is never printed or saved on this PC.
"""

import json
import re
import secrets
import subprocess
import time
import tkinter as tk
from tkinter import ttk

import bench

STATUS_NAMES = {
    1: "it can't see that network (it needs 2.4 GHz)",
    4: "joining failed (check the password)",
    6: "it was refused (check the password)",
}


def current_ssid():
    output = subprocess.run(["netsh", "wlan", "show", "interfaces"], capture_output=True, text=True).stdout
    match = re.search(r"^\s*SSID\s*:\s*(.+?)\s*$", output, re.MULTILINE)
    return match.group(1) if match else ""


def ask_wifi(default_ssid):
    answer = {}
    root = tk.Tk()
    root.title("Robot Wi-Fi")
    root.attributes("-topmost", True)
    frame = ttk.Frame(root, padding=16)
    frame.grid()
    ssid = tk.StringVar(value=default_ssid)
    password = tk.StringVar()
    ttk.Label(frame, text="Wi-Fi name").grid(row=0, column=0, sticky="w", pady=4)
    ttk.Entry(frame, textvariable=ssid, width=30).grid(row=0, column=1, pady=4)
    ttk.Label(frame, text="Password").grid(row=1, column=0, sticky="w", pady=4)
    entry = ttk.Entry(frame, textvariable=password, width=30, show="*")
    entry.grid(row=1, column=1, pady=4)

    def submit(*_):
        answer.update(ssid=ssid.get(), password=password.get())
        root.destroy()

    ttk.Button(frame, text="Send to robot", command=submit).grid(row=2, column=1, sticky="e", pady=8)
    root.bind("<Return>", submit)
    entry.focus_force()
    root.mainloop()
    return answer


def read_until(port, seconds, *words):
    """Returns the first line containing one of the words, or None."""
    deadline = time.time() + seconds
    while time.time() < deadline:
        line = port.readline().decode(errors="replace").strip()
        if any(word in line for word in words):
            return line
    return None


def main():
    answer = ask_wifi(current_ssid())
    if not answer.get("ssid") or not answer.get("password"):
        raise SystemExit("Cancelled, nothing was sent.")

    config = json.loads(bench.SECRETS.read_text()) if bench.SECRETS.exists() else {}
    token = config.get("token") or secrets.token_urlsafe(18)

    port = bench.connect_usb()
    try:
        port.reset_input_buffer()
        fields = [answer["ssid"].encode().hex(), answer["password"].encode().hex(), token]
        port.write(("wifi " + " ".join(fields) + "\n").encode())
        if read_until(port, 3, "wifi saved", "ERR") != "wifi saved":
            raise SystemExit("The robot did not accept the details.")
        print("Details saved on the robot. Waiting for it to join...")

        time.sleep(3)
        deadline = time.time() + 25
        status = None
        while time.time() < deadline:
            port.reset_input_buffer()
            port.write(b"ip\n")
            line = read_until(port, 1, "ip=")
            match = re.search(r"ip=([\d.]+) status=(\d+) rssi=(-?\d+)", line or "")
            if match:
                status = int(match.group(2))
                if status == 3:
                    bench.SECRETS.write_text(json.dumps({"host": match.group(1), "token": token}))
                    print(f"Joined. Robot is at {match.group(1)}, signal {match.group(3)} dBm.")
                    return
            time.sleep(1)
        reason = STATUS_NAMES.get(status, f"status {status}")
        raise SystemExit(f"The robot did not join {answer['ssid']}: {reason}.")
    finally:
        port.close()


if __name__ == "__main__":
    main()
