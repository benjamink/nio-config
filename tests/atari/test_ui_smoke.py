"""Drive the Atari config app in headless AltirraSDL and check the screen.

Needs: the workspace environment (source scripts/env.sh), the XEX built with
`make -f makefiles/build.mk TARGET=atari`, and an AltirraSDL build with the
embedded FujiNet-NIO device (repos/AltirraSDL).  Skips itself if any is
missing.  Run from repos/nio-config:

    python3 -m unittest discover -s tests/atari -v
"""
import os
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))

XEX = REPO / "build/atari/bin/config-nio.xex"
ALTIRRA = os.environ.get("ALTIRRA_WORKSPACE_BIN")


def need():
    if not XEX.is_file():
        raise unittest.SkipTest(f"build {XEX} first")
    if not ALTIRRA or not Path(ALTIRRA).is_file():
        raise unittest.SkipTest("ALTIRRA_WORKSPACE_BIN (source scripts/env.sh) not set or missing")
    try:
        import atari_ui  # noqa: F401
    except Exception as exc:  # pragma: no cover
        raise unittest.SkipTest(f"harness unavailable: {exc}")


class AtariUiSmoke(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        need()
        cls.tmp = tempfile.TemporaryDirectory(prefix="nio-ui-test-")
        tmp = Path(cls.tmp.name)
        (tmp / "games").mkdir()
        for name in ("alpha.atr", "beta.atr"):
            (tmp / "games" / name).write_bytes(b"\x96\x02" + bytes(1024))
        (tmp / "notes.txt").write_text("hello\n")
        from atari_ui import EmbeddedSession
        cls.s = EmbeddedSession(
            XEX, data=[("games/alpha.atr", tmp / "games/alpha.atr"),
                       ("games/beta.atr", tmp / "games/beta.atr"),
                       ("notes.txt", tmp / "notes.txt")]).start()
        cls.s.wait_for("Hosts", row=1, timeout=20)

    @classmethod
    def tearDownClass(cls):
        cls.s.stop()
        cls.tmp.cleanup()

    # The tests share one emulator run and build on each other, so they are
    # numbered to run in order.

    def test_1_first_screen(self):
        rows = self.s.screen()
        self.assertIn("FujiNet NIO Config", rows[0])
        for tab in ("Hosts", "Browse", "Drives", "Setup"):
            self.assertIn(tab, rows[1])
        self.assertIn("sd0:/", "\n".join(rows[3:21]))
        self.assertIn("Browse", rows[22])       # key hints

    def test_2_add_host_and_browse(self):
        s = self.s
        s.key("A")
        s.type("host:/")
        s.key("RETURN")
        rows = s.wait_for("host:/", row=2)
        s.key("RETURN")                         # browse the new host
        rows = s.wait_for("games", timeout=15)
        self.assertIn("host:/", rows[2])
        listing = "\n".join(rows[3:21])
        self.assertIn("notes.txt", listing)
        self.assertIn("<DIR>", listing)
        # the status row shows the size/date of the entry under the cursor
        self.assertTrue("<DIR>" in rows[23] or "bytes" in rows[23], rows[23])

    def test_3_assign_image_to_drive(self):
        s = self.s
        rows = s.screen()
        # move to "games" and enter it
        index = next(i for i, r in enumerate(rows[3:21]) if r.strip().startswith("games"))
        for _ in range(index):                  # the cursor starts on the first row
            s.key("DOWN")
        s.key("RETURN")
        rows = s.wait_for("alpha.atr", timeout=15)
        self.assertIn("host:/games/", rows[2])
        # the cursor starts on ".."; alpha.atr is next
        s.key("DOWN")
        s.key("RETURN")
        rows = s.wait_for("Assign to a drive", timeout=5)
        s.key("2")
        rows = s.wait_for("D2 <- alpha.atr", row=23, timeout=10)
        s.key("TAB")                            # Browse -> Drives
        rows = s.wait_for("D2:", timeout=5)
        d2 = next(r for r in rows if r.strip().startswith("D2:"))
        self.assertIn("alpha.atr", d2)
        self.assertIn("RW", d2)
        self.assertIn("host:/games/alpha.atr", rows[2])
        d1 = next(r for r in rows if r.strip().startswith("D1:"))
        self.assertIn("(empty)", d1)

    def test_4_eject_and_write_protect(self):
        s = self.s
        # D2, the drive just assigned, is still selected
        rows = s.screen()
        d2 = next(r for r in rows if r.strip().startswith("D2:"))
        self.assertIn("alpha.atr", d2)
        s.key("R")
        rows = s.wait_for("RO", timeout=5)
        s.key("X")
        rows = s.wait_for("(empty)", timeout=5)
        d2 = next(r for r in rows if r.strip().startswith("D2:"))
        self.assertIn("(empty)", d2)

    def test_5_setup_tab(self):
        s = self.s
        s.key("TAB")
        rows = s.wait_for("Firmware", timeout=10)
        text = "\n".join(rows)
        self.assertIn("Wi-Fi", text)
        self.assertIn("MAC", text)
        self.assertNotIn("Firmware   unknown", text)


if __name__ == "__main__":
    unittest.main()
