"""Drive the Atari config app in headless AltirraSDL (embedded FujiNet-NIO).

A small library used by atari_shot.py (command line) and
tests/atari/test_ui_smoke.py.  It starts the emulator with the embedded
FujiNet-NIO device, loads an XEX directly, and gives you keys, screenshots
and a decoded copy of the 40x24 screen, read from the program's own screen
memory through the bridge (so tests can assert on text instead of pixels).

The embedded device answers the native FujiNet endpoint but not D1: disk
SIO, so boot-disk and OPTION-to-boot runs need atari_shot_netsio.py.
"""
import argparse, os, shutil, signal, subprocess, sys, tempfile, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SDK = ROOT / "repos/AltirraSDL/src/AltirraSDL/AltirraBridge/sdk/python"
DL = 0x0600          # display list built by the app (atari_scr.c)
COLS, ROWS = 40, 24

YAML = """fujinet:
  device_name: "atari-config-shot"
boot:
  mode: "config"
  config_uri: "persist:/boot/autorun.atr"
  readonly: true
wifi:
  enabled: false
  ssid: ""
  passphrase: ""
devices:
  modem: {enabled: false, sniffer_enabled: false}
  cpm: {enabled: false, ccp_image: ""}
  printer: {enabled: false}
clock: {timezone: "UTC", enabled: true}
"""

KEYS_CTRL = {"UP": "MINUS", "DOWN": "EQUALS"}   # Ctrl+- and Ctrl+= are the Atari cursor keys


def screen_code_to_char(code):
    """ROM screen code -> a printable character ('~' for graphics glyphs)."""
    c = code & 0x7F
    if c < 64:
        return chr(c + 32)
    if c < 96:
        return "~"
    return chr(c)


def read_screen(bridge):
    """The 24 rows of text on the app's screen, from its screen memory."""
    dl = bridge.peek(DL, 3 + ROWS * 3)
    rows = []
    for r in range(ROWS):
        op = 3 + r * 3
        addr = dl[op + 1] | (dl[op + 2] << 8)
        raw = bridge.peek(addr, COLS)
        rows.append("".join(screen_code_to_char(c) for c in raw))
    return rows


def wait_screen(bridge, text, timeout=60, row=None):
    """Wait until `text` is on the screen (or on one row); returns the rows."""
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            rows = read_screen(bridge)
        except Exception:
            rows = []
        hay = rows if row is None else rows[row:row + 1]
        if any(text in line for line in hay):
            return rows
        time.sleep(.5)
    raise TimeoutError(f"{text!r} not on screen")


class EmbeddedSession:
    """One headless AltirraSDL run of an XEX with a private FujiNet data dir."""

    def __init__(self, xex, data=(), keep=None, extra=(), log=None):
        self.xex = str(xex)
        self.cfg = Path(keep) if keep else Path(tempfile.mkdtemp(prefix="nio-shot-"))
        (self.cfg / "boot").mkdir(parents=True, exist_ok=True)
        if not (self.cfg / "fujinet.yaml").exists():
            (self.cfg / "fujinet.yaml").write_text(YAML)
        for dest, src in data:
            (self.cfg / dest).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(src, self.cfg / dest)
        self.extra = list(extra)
        self.log = log
        self.proc = None
        self.bridge = None

    def start(self):
        os.environ["ATARI_FUJINET_CONFIG_DIR"] = str(self.cfg)
        os.environ["ALTIRRA_BIN"] = os.environ.get("ALTIRRA_WORKSPACE_BIN", "AltirraSDL")
        sys.path.insert(0, str(ROOT))
        from tools.atari_emulator.altirra import build_command
        ns = argparse.Namespace(
            profile=ROOT / "configs/atari/profiles/altirra-embedded-fujinet-nio.yaml",
            emulators=ROOT / "configs/atari/emulators.yaml",
            symbols=None, dry_run=False, program=self.xex, disk=[])
        cmd, env, self._tmp = build_command(ns)
        cmd += self.extra + ["--headless", "--bridge=tcp:127.0.0.1:0"]
        merged = os.environ.copy(); merged.update(env)
        before = set(Path("/tmp").glob("altirra-bridge-*.token"))
        out = open(self.log, "w") if self.log else subprocess.DEVNULL
        self.proc = subprocess.Popen(cmd, env=merged, start_new_session=True,
                                     stdout=out, stderr=subprocess.STDOUT)
        end = time.monotonic() + 20
        token = None
        while time.monotonic() < end and not token:
            new = [t for t in Path("/tmp").glob("altirra-bridge-*.token") if t not in before]
            token = max(new, key=lambda t: t.stat().st_mtime) if new else None
            time.sleep(.1)
        if not token:
            self.stop()
            raise RuntimeError("AltirraSDL bridge token not created")
        sys.path.insert(0, str(SDK))
        from altirra_bridge import AltirraBridge
        self.bridge = AltirraBridge.from_token_file(token)
        self.bridge.__enter__()
        return self

    def stop(self):
        if self.bridge:
            try: self.bridge.__exit__(None, None, None)
            except Exception: pass
            self.bridge = None
        if self.proc:
            try: os.killpg(self.proc.pid, signal.SIGTERM)
            except ProcessLookupError: pass
            try: self.proc.wait(5)
            except subprocess.TimeoutExpired:
                os.killpg(self.proc.pid, signal.SIGKILL); self.proc.wait()
            self.proc = None

    def __enter__(self): return self.start()
    def __exit__(self, *a): self.stop()

    # -- input ------------------------------------------------------------
    def key(self, name, ctrl=False):
        if name in KEYS_CTRL:
            name, ctrl = KEYS_CTRL[name], True
        self.bridge.key(name, ctrl=ctrl); time.sleep(.2)

    def type(self, text):
        self.bridge.type(text); time.sleep(.3)

    def option(self):
        self.bridge.consol(option=True); time.sleep(.3); self.bridge.consol(); time.sleep(.3)

    # -- output -----------------------------------------------------------
    def screenshot(self, path=None):
        data = self.bridge.screenshot()
        if path:
            Path(path).write_bytes(data)
        return data

    def screen(self):
        return read_screen(self.bridge)

    def wait_for(self, text, timeout=10, row=None):
        """Wait until `text` appears on the screen (or on one row)."""
        try:
            return wait_screen(self.bridge, text, timeout, row)
        except TimeoutError:
            raise TimeoutError(f"{text!r} not on screen; screen was:\n" + "\n".join(self.screen()))


def run_script(b, script, out, xex=False):
    """Replay a key script on a connected AltirraBridge `b` (shared with the
    NetSIO harness)."""
    out = Path(out)
    for word in script.split():
        k, _, v = word.partition(":")
        if k == "wait": time.sleep(float(v))
        elif k == "shot":
            (out / f"{v}.png").write_bytes(b.screenshot()); print("shot", v)
        elif k == "key":
            if v in KEYS_CTRL: b.key(KEYS_CTRL[v], ctrl=True)
            else: b.key(v)
            time.sleep(.25)
        elif k == "keyc": b.key(v, ctrl=True); time.sleep(.25)
        elif k == "type": b.type(v.replace("_", " ")); time.sleep(.4)
        elif k == "joy":
            d, _, f = v.partition("+")
            b.joy(0, d, fire=bool(f)); time.sleep(.15); b.joy(0, "centre"); time.sleep(.25)
        elif k in ("option", "start", "select"):
            b.consol(**{k: True}); time.sleep(.3); b.consol(); time.sleep(.3)
        elif k == "waitfor":
            text, _, t = v.partition("@")      # waitfor:Hosts@90 (seconds)
            wait_screen(b, text.replace("_", " "), float(t or 60))
        elif k == "regs": print("regs", b.regs())
        elif k == "peek":
            addr, ln = v.split(":"); print(addr, b.peek(int(addr, 16), int(ln)).hex())
        else: sys.exit(f"bad script word {word}")
