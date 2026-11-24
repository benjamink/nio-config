"""Host tests for amiga/bootdisk/build.py using a synthetic base disk.

The real Workbench floppies are licensed and never committed, so these build a
small fake "Workbench" ADF with xdftool and a matching test profile.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
BOOTDISK = os.path.join(HERE, "..", "..", "..", "amiga", "bootdisk")
sys.path.insert(0, BOOTDISK)
import build  # noqa: E402

PROFILE = """
name = "fake"
base_volume = "FakeWB"
output = "Fake-Boot.adf"
min_free_bytes = 1024
delete = ["Devs/junk.device", "C/Junk"]
delete_dirs = ["Rexxc"]
startup_remove = ["mount speak:"]
config_nio_dst = "Tools/config-nio"
tool_icon = "config-nio.info"
desktop_file = "ConfigNIO"
desktop_icon = "FujiNet-Config.info"
autostart_file = "WBStartup/FujiNet-Config"
autostart_icon = "FujiNet-Config.info"
installer = "wb204/Install-HD"
installer_dst = "Install-HD"
user_startup = "C:fujinet-load-resident DEVS:fujinet-nio.device fujinet-nio.device\\n"
[[package]]
src = "fujinet-nio.device"
dst = "Devs/fujinet-nio.device"
[[package]]
src = "fmount"
dst = "C/fmount"
"""


def xdf(*args):
    subprocess.run(["xdftool", *args], check=True, capture_output=True)


@unittest.skipUnless(shutil.which("xdftool"), "xdftool (amitools) not installed")
class BootdiskTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        t = self.tmp
        self.base = os.path.join(t, "base.adf")
        for name, data in (("startup", b"echo hi\nmount speak:\nLoadWB\n"),
                           ("junk", b"x" * 700)):
            with open(os.path.join(t, name), "wb") as f:
                f.write(data)
        xdf(self.base, "create", "+", "format", "FakeWB", "ofs", "+", "makedir", "C",
            "+", "makedir", "Devs", "+", "makedir", "S", "+", "makedir", "Rexxc",
            "+", "makedir", "WBStartup",
            "+", "write", os.path.join(t, "startup"), "S/Startup-Sequence",
            "+", "write", os.path.join(t, "junk"), "Devs/junk.device",
            "+", "write", os.path.join(t, "junk"), "C/Junk",
            "+", "write", os.path.join(t, "junk"), "Rexxc/RX")
        self.pkg = os.path.join(t, "pkg")
        os.makedirs(self.pkg)
        for name in ("fujinet-nio.device", "fmount"):
            with open(os.path.join(self.pkg, name), "wb") as f:
                f.write(b"\0" * 300)
        self.cfg = os.path.join(t, "config-nio")
        with open(self.cfg, "wb") as f:
            f.write(b"\0" * 900)
        self.profile = os.path.join(t, "fake.toml")
        with open(self.profile, "w") as f:
            f.write(PROFILE)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def run_build(self, **kw):
        return build.build(build.load_profile(self.profile), self.base, self.pkg,
                           self.cfg, os.path.join(self.tmp, "out"), **kw)

    def test_builds_expected_layout(self):
        out, free = self.run_build()
        _, entries = build.listing(out)
        for path in ("Devs/fujinet-nio.device", "C/fmount", "Tools/config-nio",
                     "Tools/config-nio.info", "ConfigNIO", "ConfigNIO.info",
                     "Install-HD", "Install-HD.info", "S/User-Startup",
                     "WBStartup/FujiNet-Config", "WBStartup/FujiNet-Config.info"):
            self.assertIn(path, entries)
        for path in ("Devs/junk.device", "C/Junk", "Rexxc", "Rexxc/RX"):
            self.assertNotIn(path, entries)
        self.assertGreaterEqual(free, 1024)
        seq = os.path.join(self.tmp, "seq")
        xdf(out, "read", "S/Startup-Sequence", seq)
        with open(seq) as f:
            self.assertEqual(f.read(), "echo hi\nLoadWB\n")

    def test_boot_block_preserved(self):
        out, _ = self.run_build()
        with open(self.base, "rb") as a, open(out, "rb") as b:
            self.assertEqual(a.read(1024), b.read(1024))

    def test_wrong_base_volume_rejected(self):
        xdf(self.base, "relabel", "Other")
        with self.assertRaises(build.BuildError):
            self.run_build()
        self.assertFalse(os.path.exists(os.path.join(self.tmp, "out", "Fake-Boot.adf")))

    def test_missing_delete_path_rejected(self):
        xdf(self.base, "delete", "C/Junk")
        with self.assertRaisesRegex(build.BuildError, "C/Junk"):
            self.run_build()

    def test_checksum_mismatch_rejected_unless_allowed(self):
        prof = build.load_profile(self.profile)
        prof["base_sha256"] = "0" * 64
        args = (prof, self.base, self.pkg, self.cfg, os.path.join(self.tmp, "out"))
        with self.assertRaisesRegex(build.BuildError, "sha256"):
            build.build(*args)
        build.build(*args, allow_unknown_base=True)

    def test_overflow_rejected_and_output_removed(self):
        with open(self.cfg, "wb") as f:
            f.write(b"\0" * 900_000)
        with self.assertRaises(build.BuildError):
            self.run_build()
        self.assertFalse(os.path.exists(os.path.join(self.tmp, "out", "Fake-Boot.adf")))

    def test_boot_block_checksum_and_scratch_build(self):
        blk = build.boot_block(0)
        self.assertEqual(blk[:4], b"DOS\0")
        total = 0
        for i in range(0, 1024, 4):
            total += int.from_bytes(blk[i:i + 4], "big")
            if total > 0xFFFFFFFF:
                total = (total & 0xFFFFFFFF) + 1
        self.assertEqual(total, 0xFFFFFFFF)
        prof = {"name": "s", "scratch": True, "volume": "Scratch", "output": "S.adf",
                "dirs": ["C", "S", "Tools"], "config_nio_dst": "Tools/config-nio",
                "startup_sequence": "Tools/config-nio\n", "min_free_bytes": 1024,
                "package": [{"src": "fmount", "dst": "C/fmount"}]}
        out, _ = build.build(prof, None, self.pkg, self.cfg,
                             os.path.join(self.tmp, "out2"))
        with open(out, "rb") as f:
            self.assertEqual(f.read(1024), blk)
        _, entries = build.listing(out)
        for path in ("C/fmount", "Tools/config-nio", "S/Startup-Sequence"):
            self.assertIn(path, entries)

    def test_real_profiles_parse(self):
        for name in ("wb204",):
            prof = build.load_profile(name)
            for key in ("name", "base_volume", "output", "user_startup",
                        "config_nio_dst", "installer", "package"):
                self.assertIn(key, prof)
            self.assertTrue(os.path.isfile(os.path.join(BOOTDISK, prof["installer"])))

    def test_free_profiles_ship_no_commodore_files(self):
        # Freely distributable disks: built from scratch (no base disk), and
        # nothing from Workbench (Mount, Run, FastFileSystem, ...) is packaged.
        project = {"fujinet-nio.device", "fujinet-disk.device",
                   "fujinet-serial.device", "fujinet-nio-serial",
                   "fujinet-nio-baud", "fujinet-load-resident", "fmount",
                   "fumount"}
        for name in ("free",):
            prof = build.load_profile(name)
            self.assertTrue(prof.get("scratch"))
            self.assertNotIn("base_volume", prof)
            for item in prof["package"]:
                self.assertIn(item["src"], project)
            self.assertNotIn("Mount", prof["startup_sequence"])

    def test_free_profile_sets_38400_before_config(self):
        # 38400 is the rate verified on Kickstart 1.3 and 3.1 (stock and
        # PiStorm); the FujiNet must be set to match.
        prof = build.load_profile("free")
        lines = prof["startup_sequence"].splitlines()
        self.assertIn("C:fujinet-nio-baud 38400", lines)
        self.assertLess(lines.index("C:fujinet-nio-baud 38400"),
                        lines.index("Tools/config-nio"))
        baud = [p for p in prof["package"] if p["src"] == "fujinet-nio-baud"]
        self.assertEqual(baud, [{"src": "fujinet-nio-baud",
                                 "dst": "C/fujinet-nio-baud",
                                 "from": "tools"}])


if __name__ == "__main__":
    unittest.main()
