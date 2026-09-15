"""A small window onto the living organism.

Serves the panel and drives the real binary behind it, so what the page shows is
the organism as it stands rather than a snapshot of when someone remembered to
export one. Standard library only -- the project takes no dependencies, and its
tooling has no business taking them either.

    python tools/live.py          then open http://localhost:7331
"""
import json
import os
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = os.path.join(ROOT, "razar.exe" if os.name == "nt" else "razar")
PANEL = os.path.join(ROOT, "docs", "live.html")
PORT = 7331

# One action at a time. The organism has one memory and two runs writing to it
# would corrupt the nest, so the lock is correctness rather than politeness.
LOCK = threading.Lock()
BUSY = {"what": None, "since": 0.0, "log": []}


def run(args, timeout=900):
    return subprocess.run([BINARY] + args, cwd=ROOT, capture_output=True,
                          text=True, timeout=timeout, encoding="utf-8",
                          errors="replace")


def state():
    """What it is, right now. Reads a saved mind and changes nothing."""
    p = run(["dash"], timeout=120)
    if p.returncode != 0 or not p.stdout.strip().startswith("{"):
        return {"error": (p.stderr or p.stdout or "no output").strip()[:400]}
    try:
        return json.loads(p.stdout)
    except json.JSONDecodeError as e:
        return {"error": "el volcado no se pudo leer: %s" % e}


ACTIONS = {
    # name        argv                                          what it means
    # the panel is read in Spanish, so what it says while working is too
    "feed":    (["feed", "feed/nuevo-testamento.txt"],
                "comiendo el Nuevo Testamento"),
    "feed-c":  (["feed", "feed/organismo.c"],
                "comiendo el organismo ajedrecista"),
    "senses":  (["senses", "25", "feed/nuevo-testamento.txt"],
                "haciendose maneras de mirar"),
    "sleep":   (["sleep", "7"],
                "durmiendo sobre lo que vivio"),
    "think":   (["think", "feed/nuevo-testamento.txt", "9000"],
                "pensando en voz alta mientras lee"),
}


def do(name):
    """Read what it says AS IT SAYS IT.

    Waiting for the process to exit and handing over a transcript is not
    watching something think -- by the time it arrives, the thinking is over.
    So the lines are taken one at a time, and the panel polling the log sees
    them arrive."""
    argv, meaning = ACTIONS[name]
    BUSY["what"] = meaning
    BUSY["since"] = time.time()
    BUSY["log"] = []
    try:
        proc = subprocess.Popen([BINARY] + argv, cwd=ROOT,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, encoding="utf-8", errors="replace",
                                bufsize=1)
        said = []
        for line in proc.stdout:
            line = line.rstrip()
            if not line:
                continue
            said.append(line)
            BUSY["log"] = said[-16:]          # the panel reads this as it grows
        proc.wait(timeout=60)
    except Exception as e:
        BUSY["log"] = (BUSY["log"] or []) + ["se corto: %s" % e]
    finally:
        BUSY["what"] = None


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass                      # the page is the log; the console stays quiet

    def send_json(self, obj, code=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlparse(self.path).path

        if path in ("/", "/index.html"):
            try:
                with open(PANEL, "rb") as f:
                    body = f.read()
            except OSError:
                self.send_error(404, "docs/live.html is missing")
                return
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
            return

        if path == "/state":
            busy = BUSY["what"]
            payload = {
                "busy": busy,
                "for": round(time.time() - BUSY["since"], 1) if busy else 0,
                "log": BUSY["log"],
            }
            # while it is working the nest is mid-write, so the panel keeps the
            # last good reading rather than showing a torn one
            if not busy:
                payload["state"] = state()
            self.send_json(payload)
            return

        if path.startswith("/do/"):
            name = path[4:]
            if name not in ACTIONS:
                self.send_json({"error": "no existe esa accion: %s" % name}, 400)
                return
            if not LOCK.acquire(blocking=False):
                self.send_json({"error": "ya esta ocupado: %s" % BUSY["what"]}, 409)
                return

            def work():
                try:
                    do(name)
                finally:
                    LOCK.release()

            threading.Thread(target=work, daemon=True).start()
            self.send_json({"started": ACTIONS[name][1]})
            return

        self.send_error(404)


def main():
    if not os.path.exists(BINARY):
        print("no hay binario en %s: compilalo primero con make" % BINARY)
        return 1
    if not os.path.exists(PANEL):
        print("no hay tablero en %s" % PANEL)
        return 1
    srv = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    print("Razar escucha en http://localhost:%d" % PORT)
    print("Ctrl-C para frenarlo.")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
