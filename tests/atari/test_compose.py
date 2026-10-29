"""Exercise the actual 6502 URI composer over input and capacity boundaries."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from common import REPO


class ComposeTest(unittest.TestCase):
    def test_compose_6502(self):
        if not shutil.which("cl65") or not shutil.which("sim65"):
            self.skipTest("cl65 and sim65 required (source scripts/env.sh)")
        with tempfile.TemporaryDirectory(prefix="nio-compose-") as tmp:
            binary = Path(tmp) / "compose-test"
            subprocess.run(["cl65", "-t", "sim6502", "-O", "-I",
                            str(REPO / "src/platform/atari"), "-o", str(binary),
                            str(REPO / "tests/atari/compose_test.c"),
                            str(REPO / "src/platform/atari/atari_compose.s")],
                           check=True, cwd=tmp, capture_output=True, text=True)
            subprocess.run(["sim65", str(binary)], check=True, timeout=120,
                           capture_output=True, text=True)
