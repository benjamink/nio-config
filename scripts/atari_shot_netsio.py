#!/usr/bin/env python3
"""Boot an ATR through the real firmware (NetSIO hub + POSIX fujinet-nio +
AltirraSDL), replay a key script and save screenshots.  Unlike atari_shot.py
(embedded device, XEX only) D1 here is a real boot disk, so OPTION-to-boot
can be exercised.

  atari_shot_netsio.py --atr config.atr --out DIR --script "wait:8 shot:a"
      [--data rel/path=/src/file ...]

Needs the workspace environment (scripts/env.sh) and a built
repos/fujinet-nio/build/atari-fujibus-netsio-debug.
"""
import argparse, importlib.util, os, shutil, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec); sys.modules[name] = mod
    spec.loader.exec_module(mod); return mod

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--atr", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--script", default="wait:3 shot:boot")
    ap.add_argument("--data", action="append", default=[])
    ap.add_argument("--basic", action="store_true", help="BASIC cartridge present (booted without OPTION)")
    ap.add_argument("--settle", type=float, default=6.0, help="seconds to boot before the script")
    a = ap.parse_args()
    out = Path(a.out).resolve(); out.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(ROOT / "integration-tests/atari-sio"))
    shots = load("atari_ui", HERE / "atari_ui.py")
    acc = load("run_boot_acceptance", ROOT / "integration-tests/atari-sio/run_boot_acceptance.py")
    state = {}

    def after_launch(token, run):
        data = run / "fujinet-data"
        for d in a.data:
            dest, src = d.split("=", 1)
            (data / dest).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(src, data / dest)
        state["data"] = data

    def inspector(token, run):
        sys.path[:0] = [str(acc.SDK)]
        from altirra_bridge import AltirraBridge
        with AltirraBridge.from_token_file(token) as b:
            shots.run_script(b, a.script, out)
        state["done"] = True
        return {}

    try:
        acc.run_netsio(a.settle, out, boot_atr=Path(a.atr).resolve(), inspector=inspector,
                       explicit_cold_reset=True, after_launch=after_launch,
                       basic=True if a.basic else None)
    except Exception as exc:
        if not state.get("done"):
            raise
        print("(post-run checks skipped:", type(exc).__name__ + ")")
    print("data dir:", state.get("data"))

if __name__ == "__main__":
    main()
