#!/usr/bin/env python3
"""Boot an ATR through the real firmware (NetSIO hub + POSIX fujinet-nio +
AltirraSDL), then drive the app: a key script from the command line, or a
callback from a test (see tests/atari/test_boot.py).  Unlike atari_shot.py
(embedded device, XEX only) D1 here is a real boot disk, so PicoBoot loading,
OPTION-to-boot, RESET and XEX wrapping can be exercised.

  atari_shot_netsio.py --atr config.atr --out DIR --script "waitfor:Hosts@200 shot:a"
      [--data rel/path=/src/file ...] [--basic]

Needs the workspace environment (scripts/env.sh) and a built
repos/fujinet-nio/build/atari-fujibus-netsio-debug.  The disk takes about 20
to 40 seconds to load, so scripts should use waitfor: rather than fixed waits.
"""
import argparse, importlib.util, os, shutil, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec); sys.modules[name] = mod
    spec.loader.exec_module(mod); return mod


def boot(atr, fn, data=(), basic=False, out=None, xex=None):
    """Boot `atr`, call fn(view) with an atari_ui.View on the running machine,
    and return fn's result.  `data` is [(relative path, source file)] copied
    into the firmware's data directory (the `host:/` filesystem)."""
    sys.path.insert(0, str(ROOT / "integration-tests/atari-sio"))
    sys.path.insert(0, str(HERE))
    ui = _load("atari_ui", HERE / "atari_ui.py")
    acc = _load("run_boot_acceptance", ROOT / "integration-tests/atari-sio/run_boot_acceptance.py")
    atr = Path(atr).resolve()
    data = [(d, Path(s_).resolve()) for d, s_ in data]
    out = (Path(out) if out else Path(tempfile.mkdtemp(prefix="nio-boot-"))).resolve()
    out.mkdir(parents=True, exist_ok=True)
    state = {}

    def after_launch(token, run):
        d = run / "fujinet-data"
        for dest, src in data:
            (d / dest).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, d / dest)
        state["data"] = d

    def inspector(token, run):
        sys.path[:0] = [str(acc.SDK)]
        from altirra_bridge import AltirraBridge
        with AltirraBridge.from_token_file(token) as b:
            view = ui.View()
            view.bridge = b
            view.xex = str(xex or ROOT / "repos/nio-config/build/atari/bin/config-nio.xex")
            try:
                state["result"] = fn(view)
            except BaseException as exc:      # reported after the emulator is cleaned up
                state["error"] = exc
        state["done"] = True
        return {}

    cwd = os.getcwd()
    os.chdir(ROOT)          # the profile paths in run_boot_acceptance are relative to it
    try:
        acc.run_netsio(6.0, out, boot_atr=atr, inspector=inspector,
                       explicit_cold_reset=True, after_launch=after_launch,
                       basic=True if basic else None)
    except Exception:
        if not state.get("done"):
            raise
        # run_netsio's own post-run trace checks are for the production boot
        # image's golden trace and do not apply to other disks.
    finally:
        os.chdir(cwd)
    if "error" in state:
        raise state["error"]
    return state.get("result")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--atr", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--script", default="waitfor:Hosts@200 shot:boot")
    ap.add_argument("--data", action="append", default=[], metavar="DEST=SRC")
    ap.add_argument("--basic", action="store_true", help="BASIC cartridge present (booted without OPTION)")
    a = ap.parse_args()
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    data = [tuple(d.split("=", 1)) for d in a.data]

    def run(view):
        sys.modules["atari_ui"].run_script(view.bridge, a.script, out)

    boot(a.atr, run, data, a.basic, out)


if __name__ == "__main__":
    main()
