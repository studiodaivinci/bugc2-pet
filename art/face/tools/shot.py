"""Screenshot a local page in headless Chrome, wait for document.title == 'ready', print console errors.
    python shot.py <url> <out.png> [--w 1440] [--h 900] [--full] [--wait 20] [--eval "js"]
"""
import argparse, base64, json, os, shutil, subprocess, sys, tempfile, time, urllib.request
from websockets.sync.client import connect

CHROME = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
ap = argparse.ArgumentParser()
ap.add_argument("url"); ap.add_argument("out")
ap.add_argument("--w", type=int, default=1440); ap.add_argument("--h", type=int, default=900)
ap.add_argument("--full", action="store_true"); ap.add_argument("--wait", type=float, default=20)
ap.add_argument("--eval", default=None); ap.add_argument("--dpr", type=float, default=1)
a = ap.parse_args()
prof = tempfile.mkdtemp(prefix="shot-")
proc = subprocess.Popen([CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--no-first-run",
                         "--no-default-browser-check", "--mute-audio", f"--user-data-dir={prof}",
                         "--remote-debugging-port=0", f"--window-size={a.w},{a.h}", "about:blank"],
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
n = 0
def call(ws, method, **params):
    global n
    n += 1
    ws.send(json.dumps({"id": n, "method": method, "params": params}))
    while True:
        m = json.loads(ws.recv(timeout=60))
        if m.get("id") == n:
            if "error" in m: raise RuntimeError(m["error"])
            return m.get("result", {})
        meth = m.get("method")
        if meth == "Runtime.exceptionThrown":
            d = m["params"]["exceptionDetails"]
            print("EXC:", d.get("exception", {}).get("description", d.get("text")))
        elif meth == "Runtime.consoleAPICalled" and m["params"]["type"] in ("error", "warning", "log"):
            print("CONSOLE", m["params"]["type"], " ".join(str(x.get("value", x.get("description", ""))) for x in m["params"]["args"]))
try:
    pf = os.path.join(prof, "DevToolsActivePort"); end = time.time() + 20
    while not (os.path.exists(pf) and os.path.getsize(pf)) and time.time() < end: time.sleep(0.1)
    port = open(pf).readline().strip()
    ws_url = next(t["webSocketDebuggerUrl"] for t in json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/json/list")) if t["type"] == "page")
    with connect(ws_url, max_size=None) as ws:
        call(ws, "Emulation.setDeviceMetricsOverride", width=a.w, height=a.h, deviceScaleFactor=a.dpr, mobile=False)
        call(ws, "Runtime.enable"); call(ws, "Page.enable")
        call(ws, "Page.navigate", url=a.url)
        end = time.time() + a.wait
        while time.time() < end:
            r = call(ws, "Runtime.evaluate", expression="document.title")
            if r.get("result", {}).get("value") == "ready": break
            time.sleep(0.3)
        else:
            print("TIMEOUT waiting for ready")
        if a.eval:
            r = call(ws, "Runtime.evaluate", expression=a.eval, awaitPromise=True, returnByValue=True)
            print("EVAL:", json.dumps(r.get("result", {}).get("value"))[:2000])
        time.sleep(0.3)
        shot = {"format": "png"}
        if a.full:
            hgt = min(call(ws, "Page.getLayoutMetrics")["cssContentSize"]["height"], 16000)
            shot.update(captureBeyondViewport=True, clip={"x": 0, "y": 0, "width": a.w, "height": hgt, "scale": 1})
        data = call(ws, "Page.captureScreenshot", **shot)["data"]
        open(a.out, "wb").write(base64.b64decode(data))
        print("saved", a.out)
finally:
    proc.terminate()
    try: proc.wait(10)
    except subprocess.TimeoutExpired: proc.kill()
    shutil.rmtree(prof, ignore_errors=True)
