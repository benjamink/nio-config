"""Exercise the actual 6502 URI composer under soft65c02."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from common import REPO


class ComposeTest(unittest.TestCase):
    def test_compose_6502(self):
        if not shutil.which("cl65") or not shutil.which("soft65c02_unit"):
            self.skipTest("cl65 and soft65c02_unit required (source scripts/env.sh)")
        with tempfile.TemporaryDirectory(prefix="nio-compose-") as tmp:
            result = subprocess.run(["soft65c02_unit", "-b", str(Path(tmp) / "build"),
                                     "-i", str(REPO / "tests/atari/compose_test.yaml")],
                                    timeout=120, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
