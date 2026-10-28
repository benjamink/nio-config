#!/usr/bin/env python3
"""Run the Atari config XEX in headless AltirraSDL with the embedded
FujiNet-NIO device, replay a key script, and save screenshots.

  atari_shot.py --xex build/atari/bin/config-nio.xex --out /tmp/shots \
      --script "wait:4 shot:boot key:DOWN shot:down"

Script words: wait:<seconds>  shot:<name>  key:<NAME> (UP and DOWN are
the Atari Ctrl+arrow keys)  keyc:<NAME> (with Ctrl)  type:<text> (_ is a
space)  joy:<dir>[+fire]  option|start|select  regs  peek:<hexaddr>:<len>.
--data DEST=SRC copies files into the FujiNet data directory first (the
`host:/` filesystem), and --dump prints the decoded screen at the end.
For a boot ATR through the real firmware use atari_shot_netsio.py.
"""
import argparse, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from atari_ui import EmbeddedSession, run_script


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xex", required=True, help="load this XEX directly")
    ap.add_argument("--out", required=True)
    ap.add_argument("--script", default="wait:4 shot:boot")
    ap.add_argument("--data", action="append", default=[], metavar="DEST=SRC")
    ap.add_argument("--keep", help="reuse this FujiNet data dir (state persists)")
    ap.add_argument("--extra", action="append", default=[], help="extra AltirraSDL args, as --extra=--flag")
    ap.add_argument("--dump", action="store_true", help="print the decoded screen at the end")
    a = ap.parse_args()
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    data = [tuple(d.split("=", 1)) for d in a.data]
    with EmbeddedSession(a.xex, data, a.keep, a.extra, out / "altirra.log") as s:
        run_script(s.bridge, a.script, out, True)
        if a.dump:
            print("\n".join(s.screen()))
        print("data dir:", s.cfg)


if __name__ == "__main__":
    main()
