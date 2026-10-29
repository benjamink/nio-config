"""Shared pieces for the Atari UI tests."""
import os
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))

XEX = REPO / "build/atari/bin/config-nio.xex"
ALTIRRA = os.environ.get("ALTIRRA_WORKSPACE_BIN")
# 2024-05-01 12:30:00 UTC: the modification time given to every test file, so
# the dates on screen (and the screenshots) never change.
FIXED_MTIME = 1714566600
FIXED_DATE = "2024-05-01 12:30"


def need_emulator():
    if not XEX.is_file():
        raise unittest.SkipTest(f"build {XEX} first (make -f makefiles/build.mk TARGET=atari)")
    if not ALTIRRA or not Path(ALTIRRA).is_file():
        raise unittest.SkipTest("ALTIRRA_WORKSPACE_BIN (source scripts/env.sh) not set or missing")


def make_files(root, spec):
    """Create {relative path: size} under root with the fixed mtime; returns the
    (dest, src) list EmbeddedSession wants."""
    out = []
    for rel, size in spec.items():
        path = Path(root) / "src" / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"\x96\x02" + bytes(max(size - 2, 0)))
        os.utime(path, (FIXED_MTIME, FIXED_MTIME))
        out.append((rel, path))
    # directories get the same time (deepest first, after their files exist)
    for d in sorted((p for p in (Path(root) / "src").rglob("*") if p.is_dir()), reverse=True):
        os.utime(d, (FIXED_MTIME, FIXED_MTIME))
    return out


def start(root, spec, extra=()):
    from atari_ui import EmbeddedSession
    s = EmbeddedSession(XEX, data=make_files(root, spec), extra=extra).start()
    s.wait_for("Hosts", row=1, timeout=20)
    return s


def browse_host(s, host):
    """From the Hosts tab: add `host`, open it in Browse."""
    s.key("A")
    s.type(host)
    s.key("RETURN")
    s.wait_for(host, row=2, timeout=5)
    s.key("RETURN")
    s.wait_for(host.rstrip("/"), row=2, timeout=15)


def enter(s, name):
    """Open directory `name` in the Browse list."""
    s.select(name)
    s.key("RETURN")
    s.wait_for(name.rstrip("/"), row=2, timeout=15)


def row_text(rows, prefix):
    return next(r for r in rows if r.strip().startswith(prefix))
