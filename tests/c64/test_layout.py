"""The C64 program must load where BASIC's SYS line says and leave room before the stack."""
import unittest

from common import REPO
from check_c64_layout import check, segments

CONFIG = """
SYMBOLS {
    __STACKSIZE__: type = weak, value = $0800; # 2k stack
    __HIMEM__:     type = weak, value = $D000;
}
"""

# cc65's BASIC stub: link, line 2021, SYS 2061 (which is $080D), end of line, end of program.
PRG = bytes([0x01, 0x08, 0x0B, 0x08, 0xE5, 0x07, 0x9E]) + b"2061" + bytes(3)


def map_text(last_end, startup=0x080D):
    return f"""Modules list:
-------------

Segment list:
-------------
Name                   Start     End    Size  Align
----------------------------------------------------
ZEROPAGE              000002  000037  000036  00001
LOADADDR              0007FF  000800  000002  00001
EXEHDR                000801  00080C  00000C  00001
STARTUP               {startup:06X}  00083F  000033  00001
CODE                  000840  00642F  005BF0  00001
BSS                   00A393  00B703  001371  00001
LOWMEM                00B704  {last_end:06X}  00059B  00001


Exports list by name:
"""


class C64Layout(unittest.TestCase):
    def test_reads_the_segment_list(self):
        rows = segments(map_text(0xBC9E))
        self.assertEqual(rows["STARTUP"], (0x080D, 0x083F))
        self.assertEqual(rows["LOWMEM"], (0xB704, 0xBC9E))

    def test_headroom_is_measured_to_the_stack(self):
        # The stack takes $0800 below HIMEM $D000, so the last usable byte is $C7FF.
        self.assertEqual(check(PRG, map_text(0xBC9E), CONFIG), (0xBC9E, 0xC7FF - 0xBC9E))

    def test_a_program_that_leaves_too_little_room_is_refused(self):
        check(PRG, map_text(0xC7FF - 512), CONFIG)
        with self.assertRaises(ValueError):
            check(PRG, map_text(0xC7FF - 511), CONFIG)
        with self.assertRaises(ValueError):
            check(PRG, map_text(0xC7FF), CONFIG)

    def test_a_program_that_cannot_start_is_refused(self):
        with self.assertRaises(ValueError):        # not loaded at $0801
            check(b"\x00\x08" + PRG[2:], map_text(0xBC9E), CONFIG)
        with self.assertRaises(ValueError):        # no SYS line
            check(PRG[:6] + b"\x99" + PRG[7:], map_text(0xBC9E), CONFIG)
        with self.assertRaises(ValueError):        # SYS goes somewhere other than the startup code
            check(PRG, map_text(0xBC9E, startup=0x0810), CONFIG)
        with self.assertRaises(ValueError):
            check(b"\x01\x08", map_text(0xBC9E), CONFIG)

    def test_the_linker_configuration_names_the_limits(self):
        with self.assertRaises(ValueError):
            check(PRG, map_text(0xBC9E), "SYMBOLS {}")

    def test_the_built_program_passes(self):
        prg = REPO / "build/c64/bin/config-nio.prg"
        if not prg.exists():
            self.skipTest(f"build {prg} first (make TARGET=c64 FUJINET_NIO_LIB=../fujinet-nio-lib)")
        check(prg.read_bytes(), prg.with_suffix(".map").read_text(),
              (REPO / "makefiles/cc65-c64-nio.cfg").read_text())


if __name__ == "__main__":
    unittest.main()
