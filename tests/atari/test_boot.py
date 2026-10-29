"""Boot-path tests: the default boot ATR through PicoBoot and the real
firmware (NetSIO hub + POSIX fujinet-nio + AltirraSDL).

Slow (each boots a disk, 30-60 s), so they run only when asked:

    NIO_ATARI_BOOT=1 python3 -m unittest test_boot -v        # from tests/atari

Needs the workspace environment (source scripts/env.sh), the XEX built, `dir2atr`
on PATH, a built repos/fujinet-nio/build/atari-fujibus-netsio-debug (with the
XexBoot image type for the XEX test), and nio-core-apps' proctest.xex (build it
with `make TARGET=atari` in repos/nio-core-apps) for the OPTION-boot tests.
"""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from common import REPO, XEX, need_emulator

ROOT = REPO.parents[1]
PICOBOOT = ROOT / "repos/MyPicoDOS/picobootcode-dd.bin"
PROCTEST = ROOT / "repos/nio-core-apps/build/atari/bin/proctest.xex"
NETSIO_FW = ROOT / "repos/fujinet-nio/build/atari-fujibus-netsio-debug/fujinet-nio"


def need_boot():
    need_emulator()
    if not os.environ.get("NIO_ATARI_BOOT"):
        raise unittest.SkipTest("set NIO_ATARI_BOOT=1 to run the slow boot tests")
    if not shutil.which("dir2atr"):
        raise unittest.SkipTest("dir2atr not on PATH")
    for path in (PICOBOOT, NETSIO_FW):
        if not path.is_file():
            raise unittest.SkipTest(f"{path} missing")


def make_atr(tmp, xex, name="autorun.xex"):
    """A PicoBoot disk holding one program, built the way nio-core-apps does."""
    stage = Path(tmp) / ("stage-" + Path(xex).stem)
    stage.mkdir()
    shutil.copy(xex, stage / name)
    atr = Path(tmp) / (Path(xex).stem + ".atr")
    subprocess.run(["dir2atr", "-m", "-D", "-B", str(PICOBOOT), str(atr), str(stage)],
                   check=True, capture_output=True)
    return atr


def boot(*a, **k):
    import atari_shot_netsio
    return atari_shot_netsio.boot(*a, **k)


class BootTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        need_boot()
        cls.tmp = tempfile.TemporaryDirectory(prefix="nio-boot-test-")
        cls.config_atr = make_atr(cls.tmp.name, XEX)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def check_running_and_basic_off(self, v):
        v.wait_for("Hosts", row=1, timeout=240)
        portb = v.bridge.peek(0xD301, 1)[0]
        self.assertTrue(portb & 0x02, f"BASIC still enabled (PORTB={portb:#04x})")
        self.assertEqual(v.bridge.peek(0x03F8, 1)[0], 1, "BASICF not set")
        self.assertEqual(v.bridge.peek(0x02E6, 1)[0], 0xBC, "MEMTOP not moved")

    def test_boots_without_basic(self):
        boot(self.config_atr, self.check_running_and_basic_off, basic=False)

    def test_boots_with_basic_present_and_turns_it_off(self):
        boot(self.config_atr, self.check_running_and_basic_off, basic=True)

    def test_reset_restarts_the_app_with_basic_still_off(self):
        def scenario(v):
            self.check_running_and_basic_off(v)
            v.key("TAB")
            v.wait_for("RET Open", row=22, timeout=10)      # on the Browse tab
            v.bridge.warm_reset()
            v.wait_for("RET Browse A Add", row=22, timeout=60)   # back on Hosts
            self.check_running_and_basic_off(v)
        boot(self.config_atr, scenario, basic=True)

    def option_boot(self, program, expect):
        """Assign `program` (a boot ATR or an XEX) to D1 from the Browse tab,
        press OPTION, and wait for `expect` on the OS screen."""
        def scenario(v):
            v.wait_for("Hosts", row=1, timeout=240)
            v.key("A"); v.type("host:/"); v.key("RETURN")
            v.wait_for("host:/", row=2, timeout=10)
            v.key("RETURN")
            v.wait_for(program.name, timeout=60)
            v.select(program.name)
            v.key("RETURN")
            v.wait_for("Assign to a drive", timeout=30)
            v.key("1")
            v.wait_for("D1 <-", row=23, timeout=30)
            v.option()
            v.wait_os(expect, timeout=240)
        boot(self.config_atr, scenario, data=[(program.name, program)], basic=False)

    @unittest.skipUnless(PROCTEST.is_file(), "build nio-core-apps (proctest.xex) first")
    def test_option_boots_an_assigned_boot_disk(self):
        atr = make_atr(self.tmp.name, PROCTEST)
        self.option_boot(atr, "PROCTEST")

    @unittest.skipUnless(PROCTEST.is_file(), "build nio-core-apps (proctest.xex) first")
    def test_option_boots_an_assigned_xex_through_the_wrapper(self):
        xex = Path(self.tmp.name) / "proctest.xex"
        shutil.copy(PROCTEST, xex)
        self.option_boot(xex, "PROCTEST")


if __name__ == "__main__":
    unittest.main()
