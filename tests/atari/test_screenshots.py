"""Whole-screen regression: screenshots of the main screens compared with
saved references, so offset, overlapping or mis-coloured text fails a test
(the text tests cannot see colours or fine layout).

    python3 -m unittest test_screenshots -v          # compare
    UPDATE_GOLDEN=1 python3 -m unittest test_screenshots   # accept the current look

References live in tests/atari/golden/.  A failing comparison writes the actual
image to tests/atari/golden/_actual/ for a side-by-side look; review it, then
re-run with UPDATE_GOLDEN=1 if the change is intended.  The Setup tab is not
captured (firmware version, MAC).  Screens are taken before any idle animation.
"""
import os
import tempfile
import time
import unittest
from pathlib import Path

from common import browse_host, make_files, need_emulator, start

GOLDEN = Path(__file__).resolve().parent / "golden"
SPEC = {
    "shots/alpha.atr": 184320,
    "shots/beta.atr": 92160,
    "shots/readme.txt": 1200,
    "shots/games/g.atr": 100,
}


def png_pixels(data):
    from PIL import Image
    import io
    im = Image.open(io.BytesIO(data)).convert("RGB")
    return im.size, im.tobytes()


class Screenshots(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        need_emulator()
        try:
            import PIL  # noqa: F401
        except ImportError:
            raise unittest.SkipTest("Pillow not installed")
        cls.tmp = tempfile.TemporaryDirectory(prefix="nio-shots-")
        cls.s = start(cls.tmp.name, SPEC)
        GOLDEN.mkdir(exist_ok=True)

    @classmethod
    def tearDownClass(cls):
        cls.s.stop()
        cls.tmp.cleanup()

    def check(self, name):
        time.sleep(.6)                       # let the last redraw finish
        data = self.s.screenshot()
        ref = GOLDEN / f"{name}.png"
        if os.environ.get("UPDATE_GOLDEN") or not ref.exists():
            ref.write_bytes(data)
            if not os.environ.get("UPDATE_GOLDEN"):
                self.skipTest(f"no reference for {name}: wrote {ref}")
            return
        (size_a, px_a), (size_b, px_b) = png_pixels(data), png_pixels(ref.read_bytes())
        if size_a != size_b or px_a != px_b:
            actual = GOLDEN / "_actual"
            actual.mkdir(exist_ok=True)
            (actual / f"{name}.png").write_bytes(data)
            differing = sum(1 for a, b in zip(px_a, px_b) if a != b) if size_a == size_b else -1
            self.fail(f"{name}: screen differs from {ref} ({differing} bytes); actual saved "
                      f"to {actual / (name + '.png')}")

    # one emulator for all: the screens follow one another

    def test_1_hosts(self):
        self.check("hosts")

    def test_2_browse(self):
        browse_host(self.s, "host:/shots/")
        self.s.wait_for("alpha.atr", timeout=15)
        self.check("browse")

    def test_3_assign_dialog_and_drives(self):
        s = self.s
        s.select("alpha.atr")
        s.key("RETURN")
        s.wait_for("Assign to a drive", timeout=10)
        self.check("assign-dialog")
        s.key("1")
        s.wait_for("D1 <-", row=23, timeout=10)
        s.select("beta.atr")
        s.key("RETURN")
        s.wait_for("Assign to a drive", timeout=10)
        s.key("R")
        s.key("2")
        s.wait_for("D2 <-", row=23, timeout=10)
        s.key("TAB")
        s.wait_for("D2:", timeout=5)
        self.check("drives")

    def test_4_host_edit_prompt(self):
        s = self.s
        s.key("TAB"); s.key("TAB")                       # Setup, Hosts
        s.wait_for("RET Browse A Add", row=22, timeout=10)
        s.key("A")
        s.type("tnfs://example.org/atari")
        s.wait_for("example.org/atari", row=2, timeout=5)
        self.check("host-prompt")
        s.key("ESC")


if __name__ == "__main__":
    unittest.main()
