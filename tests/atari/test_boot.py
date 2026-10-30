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
import time
import unittest
from pathlib import Path

from common import REPO, XEX, need_emulator

ROOT = REPO.parents[1]
PICOBOOT = ROOT / "repos/MyPicoDOS/picobootcode-dd.bin"
PROCTEST = ROOT / "repos/nio-core-apps/build/atari/bin/proctest.xex"
BANKED_XEX = REPO / "build/atari-banked/bin/config-nio.xex"
BOOTSTRAP = REPO / "build/atari-banked/bin/loader.xex"
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


def make_selecting_atr(tmp):
    """Build the production manifest and PicoBoot disk in an isolated directory."""
    out = Path(tmp) / "production-boot"
    stage = Path(tmp) / "production-stage"
    subprocess.run(["make", "-C", str(ROOT / "repos/nio-core-apps"),
                    "TARGET=atari", "boot-disk", f"BOOT_DISK_OUTPUT_DIR={out}",
                    f"BOOT_DISK_STAGE_DIR={stage}"], check=True, capture_output=True)
    return out / "autorun.atr"


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

    def check_selecting_boot(self, ram_kb, banked):
        self.assertTrue(BANKED_XEX.is_file() and BOOTSTRAP.is_file(),
                        "build both Atari variants with make atari-banked")
        atr = make_selecting_atr(self.tmp.name)
        miner = Path(self.tmp.name) / "Miner.atr"
        other = Path(self.tmp.name) / "Other.atr"
        miner.write_bytes(b"not mounted")
        other.write_bytes(b"not mounted")
        def scenario(v):
            self.check_running_and_basic_off(v)
            portb = v.bridge.peek(0xD301, 1)[0]
            self.assertEqual(bool(portb & 0x10), not banked,
                             f"wrong memory layout, PORTB={portb:#04x}")
            for label, hint in (("Browse", "RET Open"), ("Drives", "RET Pick"),
                                ("Setup", "RET Join Wi-Fi"), ("Hosts", "RET Browse A Add")):
                v.key("TAB")
                v.wait_for(hint, row=22, timeout=30)
                if label == "Setup":
                    v.key("R")
                    v.wait_for("Refreshed", row=23, timeout=30)
            if banked:
                v.key("A"); v.type("host:/"); v.key("RETURN")
                v.wait_for("host:/", row=2, timeout=20)
                v.key("RETURN")
                v.wait_for("Miner.atr", timeout=30)
                v.key("F"); v.type("*MiNeR*"); v.key("RETURN")
                v.wait_for("Filter:", row=21, timeout=30)
                self.assertIn("Miner.atr", "".join(v.screen()[3:21]))
                self.assertNotIn("Other.atr", "".join(v.screen()[3:21]))
            else:
                v.key("TAB")
            v.wait_for("RET Open", row=22, timeout=30)
            v.bridge.warm_reset()
            v.wait_for("RET Browse A Add", row=22, timeout=60)
            self.check_running_and_basic_off(v)
            portb = v.bridge.peek(0xD301, 1)[0]
            self.assertEqual(bool(portb & 0x10), not banked)
        boot(atr, scenario, basic=True, ram_kb=ram_kb,
             data=[("Miner.atr", miner), ("Other.atr", other)],
             xex=BANKED_XEX if banked else XEX)

    def test_selector_boots_standard_on_64k(self):
        self.check_selecting_boot(64, False)

    def test_selector_boots_banked_on_128k(self):
        self.check_selecting_boot(128, True)

    def test_selector_boots_banked_on_1088k(self):
        self.check_selecting_boot(1088, True)

    def test_standard_reuses_animated_long_name(self):
        atr = make_selecting_atr(self.tmp.name)
        evidence = Path(self.tmp.name) / "standard-cache-evidence"
        name = "f000_" + "x" * 40 + "tail.atr"
        source = Path(self.tmp.name) / name
        source.write_bytes(b"fixture")
        def scenario(v):
            v.wait_for("Hosts", row=1, timeout=240)
            v.key("A"); v.type("host:/long/"); v.key("RETURN")
            v.wait_for("host:/long/", row=2, timeout=20)
            v.key("RETURN"); v.wait_for("f000_", timeout=30)
            def page_calls():
                log = (evidence / "netsio/hub.log").read_text(errors="replace")
                return sum("ATD -> 02:DATA_BLOCK" in line and " FE 07 " in line
                           for line in log.splitlines())
            initial = page_calls()
            v.wait_for("tail", timeout=30)
            animated = page_calls()
            self.assertGreater(animated, initial, "animation did not fetch the full name")
            v.key("RETURN"); v.wait_for("Assign to a drive", timeout=30)
            self.assertEqual(page_calls(), animated,
                             "opening the animated name performed another ListPages request")
        boot(atr, scenario, data=[("long/" + name, source)], ram_kb=64, out=evidence)

    def test_banked_long_names_cache_and_eviction(self):
        from atari_ui import symbols
        atr = make_selecting_atr(self.tmp.name)
        evidence = Path(self.tmp.name) / "bank-cache-evidence"
        data = []
        for i in range(90):
            # A whole screen of 220-byte names makes each two-entry reply
            # nearly fill the 468-byte budget, including 256-byte crossings.
            suffix_len = 167 if 18 <= i < 36 else (155 if i == 0 else 45)
            name = "f%03d_" % i + "x" * 40 + "tail" + "y" * suffix_len + ".atr"
            source = Path(self.tmp.name) / name
            source.write_bytes(b"fixture")
            data.append(("long/" + name, source))

        def scenario(v):
            v.wait_for("Hosts", row=1, timeout=240)
            v.key("A"); v.type("host:/long/"); v.key("RETURN")
            v.wait_for("host:/long/", row=2, timeout=20)
            v.key("RETURN"); v.wait_for("f008_", timeout=30)
            # Restart the selected row's marquee after the full page has drawn.
            v.key("DOWN"); v.key("UP")
            used_addr = symbols(str(BANKED_XEX).replace(".xex", ".lbl"))["_pc_used"]
            used_first = v.bridge.peek16(used_addr)
            def page_calls():
                log = (evidence / "netsio/hub.log").read_text(errors="replace")
                return sum("ATD -> 02:DATA_BLOCK" in line and " FE 07 " in line
                           for line in log.splitlines())
            calls_first = page_calls()
            self.assertGreater(calls_first, 0)
            self.assertGreater(used_first, 800)
            self.assertEqual(sum(bool(r.strip()) for r in v.screen()[3:21]), 18)
            v.wait_for("tail", timeout=30)
            v.key("RETURN"); v.wait_for("Name is longer than 196", row=23, timeout=30)
            v.key("DOWN"); v.wait_for("tail", timeout=30)
            v.key("RETURN"); v.wait_for("Assign to a drive", timeout=30)
            self.assertEqual(page_calls(), calls_first,
                             "animation or opening requested an already cached name")
            v.key("1"); v.wait_for("D1 <-", row=23, timeout=30)
            v.key("TAB"); v.wait_for("RET Pick", row=22, timeout=30)
            self.assertTrue(v.screen()[2].rstrip().endswith("y" * 26 + ".atr"),
                            v.screen()[2])
            v.key("TAB"); v.wait_for("RET Join Wi-Fi", row=22, timeout=30)
            v.key("TAB"); v.wait_for("RET Browse A Add", row=22, timeout=30)
            v.key("TAB"); v.wait_for("f008_", timeout=30)
            v.key("SPACE"); v.wait_for("f026_", timeout=45)
            two_pages = v.bridge.peek16(used_addr)
            self.assertGreater(two_pages, used_first)
            v.key("B"); v.wait_for("f008_", timeout=45)
            self.assertEqual(v.bridge.peek16(used_addr), two_pages,
                             "returning to a cached screen fetched more pages")
            used = [two_pages]
            for page in range(2, 6):
                v.key("SPACE")
                v.wait_for("f%03d_" % ((page - 1) * 18 + 8), timeout=45)
                self.assertEqual(sum(bool(r.strip()) for r in v.screen()[3:21]), 18)
                used.append(v.bridge.peek16(used_addr))
            self.assertTrue(any(after < before for before, after in zip(used, used[1:])),
                            f"cache never reached its eviction boundary: {used}")
            self.assertLessEqual(v.bridge.peek16(used_addr), 8192)
            v.key("B"); v.wait_for("f062_", timeout=45)
            self.assertEqual(sum(bool(r.strip()) for r in v.screen()[3:21]), 18)
        boot(atr, scenario, data=data, ram_kb=128, xex=BANKED_XEX, out=evidence)

    def check_setup_cache(self, ram_kb):
        atr = make_selecting_atr(self.tmp.name)
        evidence = Path(self.tmp.name) / ("setup-cache-%d" % ram_kb)
        source = Path(self.tmp.name) / "cache-entry.atr"
        source.write_bytes(b"fixture")
        def scenario(v):
            v.wait_for("Hosts", row=1, timeout=240)
            log_path = evidence / "netsio/hub.log"
            def calls():
                log = log_path.read_text(errors="replace")
                return sum("ATD -> 02:DATA_BLOCK" in line for line in log.splitlines())
            initial = calls()
            self.assertGreater(initial, 0)
            for hint in ("RET Open", "RET Pick", "RET Join Wi-Fi"):
                v.key("TAB"); v.wait_for(hint, row=22, timeout=30)
            self.assertEqual(calls(), initial, "tabbing to Setup made a device request")
            setup = v.screen()[3:12]
            v.key("R"); v.wait_for("Refreshed", row=23, timeout=30)
            after_refresh = calls()
            self.assertGreater(after_refresh, initial, "R did not refresh Setup")
            self.assertEqual(v.screen()[3:12], setup)
            v.key("TAB"); v.wait_for("RET Browse A Add", row=22, timeout=30)
            v.key("A"); v.type("host:/"); v.key("RETURN")
            v.wait_for("host:/", row=2, timeout=30)
            v.key("RETURN"); v.wait_for("cache-entry.atr", timeout=30)
            v.key("TAB"); v.wait_for("RET Pick", row=22, timeout=30)
            after_browse = calls()
            v.key("TAB"); v.wait_for("RET Join Wi-Fi", row=22, timeout=30)
            self.assertEqual(calls(), after_browse, "Setup re-fetched after browsing")
            self.assertEqual(v.screen()[3:12], setup, "Browse overwrote cached Setup")
        boot(atr, scenario, ram_kb=ram_kb,
             xex=BANKED_XEX if ram_kb != 64 else XEX, out=evidence,
             data=[("cache-entry.atr", source)])

    def test_setup_cache_64k(self):
        self.check_setup_cache(64)

    def test_setup_cache_banked(self):
        self.check_setup_cache(128)

    def check_wifi_join_failure_restores_previous_network(self, ram_kb):
        import yaml
        atr = make_selecting_atr(self.tmp.name)
        evidence = Path(self.tmp.name) / ("wifi-trial-evidence-%d" % ram_kb)
        variables = {"FN_POSIX_WIFI_SIM_FAIL_SSID": "FujiNet-Test",
                     "FN_POSIX_WIFI_SIM_RESTORE_DELAY_MS": "6000",
                     "FN_POSIX_WIFI_SIM_SCAN_EDGE_CASES": "1"}
        previous = {key: os.environ.get(key) for key in variables}
        os.environ.update(variables)
        try:
            def scenario(v):
                v.wait_for("Hosts", row=1, timeout=240)
                for hint in ("RET Open", "RET Pick", "RET Join Wi-Fi"):
                    v.key("TAB"); v.wait_for(hint, row=22, timeout=30)
                # Join the first simulated network successfully.
                v.key("RETURN"); v.wait_for("Wi-Fi networks in range", row=23, timeout=30)
                rows = v.screen()
                self.assertIn("-30", rows[3])
                self.assertEqual(sum("FujiNet-Sim" in row for row in rows[3:21]), 1)
                self.assertIn("(hidden)", rows[6])
                self.assertFalse(any(row.strip() for row in rows[7:21]), rows[7:21])
                self.assertIn("Signal: dBm", rows[21])
                for _ in range(3): v.key("DOWN")
                v.key("RETURN"); v.wait_for("Wi-Fi network name (SSID)", row=21, timeout=10)
                v.key("ESC"); v.wait_for("RET Join Wi-Fi", row=22, timeout=10)
                v.key("RETURN"); v.wait_for("Wi-Fi networks in range", row=23, timeout=30)
                v.key("RETURN"); v.wait_for("Password (blank for open)", row=21, timeout=30)
                v.type("correctpass"); v.key("RETURN")
                v.wait_for("Attempting to connect", timeout=10)
                from atari_ui import symbols
                sym = symbols(Path(v.xex).with_suffix(".lbl"))
                self.assertEqual(v.bridge.peek(sym["_scr_nmi_mode"], 1)[0], 0x40)
                v.wait_for("Wi-Fi connected", row=23, timeout=60)
                v.wait_for("FujiNet-Sim", timeout=30)
                self.assertIn("Connected", "".join(v.screen()[3:21]))
                # A failed switch must leave the known-good config and reconnect it.
                v.key("RETURN"); v.wait_for("Wi-Fi networks in range", row=23, timeout=30)
                v.key("DOWN"); v.key("RETURN")
                v.wait_for("Password (blank for open)", row=21, timeout=30)
                v.type("wrongpass"); v.key("RETURN")
                v.wait_for("Wi-Fi failed; restoring previous", row=23, timeout=60)
                v.wait_for("FujiNet-Sim", timeout=30)
                self.assertIn("Connecting", "".join(v.screen()[3:21]))
                v.wait_for("Connected", row=4, timeout=25)
                self.assertIn("192.0.2.2", "".join(v.screen()[3:21]))
                self.assertIn("Wi-Fi failed; restoring previous", v.screen()[23])
                saved = yaml.safe_load((evidence / "netsio/fujinet-data/fujinet.yaml").read_text())
                self.assertEqual(saved["wifi"]["ssid"], "FujiNet-Sim")
                self.assertEqual(saved["wifi"]["passphrase"], "correctpass")
                # The user can immediately choose another network and retry.
                v.key("RETURN"); v.wait_for("Wi-Fi networks in range", row=23, timeout=30)
                v.key("DOWN"); v.key("DOWN"); v.key("RETURN")
                v.wait_for("Password (blank for open)", row=21, timeout=30)
                v.key("RETURN")
                v.wait_for("Wi-Fi connected", row=23, timeout=60)
                v.wait_for("FujiNet-Guest", timeout=30)
                self.assertIn("Connected", "".join(v.screen()[3:21]))
            boot(atr, scenario, ram_kb=ram_kb,
                 xex=BANKED_XEX if ram_kb != 64 else XEX, out=evidence)
            config = yaml.safe_load((evidence / "netsio/fujinet-data/fujinet.yaml").read_text())
            self.assertEqual(config["wifi"]["ssid"], "FujiNet-Guest")
        finally:
            for key, value in previous.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value

    def test_wifi_join_failure_restores_previous_network_banked(self):
        self.check_wifi_join_failure_restores_previous_network(128)

    def test_wifi_join_failure_restores_previous_network_64k(self):
        self.check_wifi_join_failure_restores_previous_network(64)

    def option_boot(self, program, expect, atr=None, ram_kb=None, xex=None):
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
        boot(atr or self.config_atr, scenario, data=[(program.name, program)],
             basic=False, ram_kb=ram_kb, xex=xex)

    @unittest.skipUnless(PROCTEST.is_file(), "build nio-core-apps (proctest.xex) first")
    def test_selector_banked_option_boots_xex(self):
        self.option_boot(PROCTEST, "PROCTEST", atr=make_selecting_atr(self.tmp.name),
                         ram_kb=128, xex=BANKED_XEX)

    @unittest.skipUnless(PROCTEST.is_file(), "build nio-core-apps (proctest.xex) first")
    def test_option_boots_an_assigned_boot_disk(self):
        atr = make_atr(self.tmp.name, PROCTEST)
        self.option_boot(atr, "PROCTEST")

    @unittest.skipUnless(PROCTEST.is_file(), "build nio-core-apps (proctest.xex) first")
    def test_option_boots_an_assigned_xex_through_the_wrapper(self):
        xex = Path(self.tmp.name) / "proctest.xex"
        shutil.copy(PROCTEST, xex)
        self.option_boot(xex, "PROCTEST")

    def test_mount_screen_and_error_restore(self):
        bad = Path(self.tmp.name) / "bad.atr"
        bad.write_bytes(b"not an atr")

        def scenario(v):
            from atari_ui import symbols
            v.wait_for("Hosts", row=1, timeout=240)
            v.key("A"); v.type("host:/"); v.key("RETURN")
            v.wait_for("host:/", row=2, timeout=10)
            v.key("RETURN")
            v.wait_for(bad.name, timeout=60)
            v.select(bad.name); v.key("RETURN")
            v.wait_for("Assign to a drive", timeout=30)
            v.key("1"); v.wait_for("D1 <-", row=23, timeout=30)
            v.bridge.consol(option=True)
            end = time.monotonic() + 15
            while time.monotonic() < end:
                rows = v.screen()
                if "Mounting disks" in rows[11]:
                    break
                time.sleep(.02)
            else:
                self.fail("mount screen did not appear")
            v.bridge.consol()
            self.assertTrue(all(not line.strip() for i, line in enumerate(rows) if i != 11), rows)
            sym = symbols(Path(v.xex).with_suffix(".lbl"))
            self.assertEqual(v.bridge.peek(sym["_scr_nmi_mode"], 1)[0], 0x40)
            self.assertEqual(list(v.bridge.peek(sym["_row_bg"], 24)), [0] * 24)
            v.wait_for("Mount:", row=23, timeout=60)
            self.assertIn("Browse", v.screen()[1])
            self.assertEqual(v.bridge.peek(sym["_scr_nmi_mode"], 1)[0], 0xC0)
        boot(self.config_atr, scenario, data=[(bad.name, bad)], basic=False)


if __name__ == "__main__":
    unittest.main()
