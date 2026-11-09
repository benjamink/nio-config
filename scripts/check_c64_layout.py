#!/usr/bin/env python3
"""Check a linked C64 config-nio against the memory it has.

The program loads at $0801 under BASIC's SYS line and runs with BASIC banked out, so it may
use everything up to HIMEM less the stack (makefiles/cc65-c64-nio.cfg). The linker refuses a
program that does not fit; this also refuses one that fits with too little room left, and
reports how much is: the Atari build has the same kind of guard for its splash.
"""
import re
import sys
from pathlib import Path

LOAD_ADDRESS = 0x0801
SYS_TOKEN = 0x9E
MIN_HEADROOM = 512            # bytes between the program's last byte and the stack
UNUSED_SEGMENTS = {"ZEROPAGE", "LOADADDR"}


def config_symbol(config, name):
    match = re.search(r"%s:\s*type\s*=\s*weak,\s*value\s*=\s*\$([0-9A-Fa-f]+)" % name, config)
    if not match:
        raise ValueError(f"{name} not set in the linker configuration")
    return int(match.group(1), 16)


def segments(map_text):
    """{name: (start, end)} from the map file's segment list."""
    rows = {}
    listing = re.search(r"Segment list:\n-+\n.*?\n-+\n(.*?)\n\n", map_text, re.S)
    if not listing:
        raise ValueError("no segment list in the map file")
    for line in listing.group(1).splitlines():
        fields = line.split()
        if len(fields) >= 5:
            rows[fields[0]] = (int(fields[1], 16), int(fields[2], 16))
    return rows


def check(prg, map_text, config, min_headroom=MIN_HEADROOM):
    """The program's last used address and its headroom; raises ValueError if it is wrong."""
    if len(prg) < 8 or int.from_bytes(prg[:2], "little") != LOAD_ADDRESS:
        raise ValueError("a C64 program must start with the load address $0801")
    if prg[6] != SYS_TOKEN:
        raise ValueError("the BASIC stub has no SYS line")
    digits = re.match(rb"\s*(\d+)", prg[7:])
    sys_target = int(digits.group(1)) if digits else None
    rows = segments(map_text)
    if "STARTUP" not in rows or rows["STARTUP"][0] != sys_target:
        raise ValueError(f"SYS {sys_target} does not reach the startup code")
    last = max(end for name, (start, end) in rows.items() if name not in UNUSED_SEGMENTS)
    limit = config_symbol(config, "__HIMEM__") - config_symbol(config, "__STACKSIZE__") - 1
    headroom = limit - last
    if headroom < min_headroom:
        raise ValueError(f"the program ends at ${last:04X}, leaving {headroom} bytes before the "
                         f"stack (at least {min_headroom} wanted)")
    return last, headroom


if __name__ == "__main__":
    try:
        prg_path = Path(sys.argv[1])
        last, headroom = check(prg_path.read_bytes(), prg_path.with_suffix(".map").read_text(),
                               Path(sys.argv[2]).read_text())
        print(f"C64 config-nio ends at ${last:04X}; {headroom} bytes before the stack")
    except (OSError, ValueError, IndexError) as exc:
        raise SystemExit(f"check_c64_layout: {exc}")
