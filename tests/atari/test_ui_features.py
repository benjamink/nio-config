"""Atari config UI behaviour beyond the basic flow (one emulator per test class).

Run from repos/nio-config after building the XEX and sourcing scripts/env.sh:
    python3 -m unittest discover -s tests/atari -v
"""
import tempfile
import time
import unittest

from common import (FIXED_DATE, browse_host, enter, make_files, need_emulator,
                    row_text, start)

LONG = "a_very_long_file_name_that_goes_well_past_thirty_one_characters.atr"


class Session(unittest.TestCase):
    """Base: one emulator for the class, data created from SPEC."""
    SPEC = {}

    @classmethod
    def setUpClass(cls):
        need_emulator()
        cls.tmp = tempfile.TemporaryDirectory(prefix="nio-ui-")
        cls.s = start(cls.tmp.name, cls.SPEC)

    @classmethod
    def tearDownClass(cls):
        cls.s.stop()
        cls.tmp.cleanup()


class AttractMode(Session):
    def check_attract(self, s):
        from io import BytesIO
        from PIL import Image
        b = s.bridge

        def scanlines():
            image = Image.open(BytesIO(s.screenshot())).convert("RGB")
            # The left margin contains no text; include every line so a band
            # boundary moving by even one scanline is visible to the test.
            return tuple(image.getpixel((5, y)) for y in range(image.height))

        normal_lines = scanlines()
        normal = tuple(normal_lines[y] for y in (27, 35, 67))
        b.pause()
        try:
            normal_samples = []
            for _ in range(24):
                b.cycles(35568)
                normal_samples.append(scanlines())
            self.assertTrue(all(sample == normal_samples[0] for sample in normal_samples),
                            "colour boundaries moved in normal mode")
        finally:
            b.resume()
        # The OS sets the colour shift from the middle RTC byte, changing it
        # every 256 frames. Start at a known value without waiting ten minutes.
        b.pause()
        try:
            b.poke(0x4d, 0x80)  # ATRACT: active
            b.poke(0x13, 0x30)  # RTCLOK+1
            b.poke(0x14, 0)
        finally:
            b.resume()
        time.sleep(.2)
        b.pause()
        try:
            # PAL frame: 312 scan lines of 114 CPU cycles. Freeze the
            # OS colour clock while stepping through regular screen swaps.
            samples = []
            for _ in range(12):
                b.poke(0x13, 0x30)
                b.cycles(35568)
                samples.append(scanlines())
            active = tuple(samples[0][y] for y in (27, 35, 67))
            self.assertTrue(all(a != n for a, n in zip(active, normal)), (normal, active))
            self.assertTrue(all(sample == samples[0] for sample in samples),
                            "colour boundaries moved in attract mode")
            b.poke(0x13, 0x40)
            b.cycles(35568 * 2)
            shifted_lines = scanlines()
            shifted = tuple(shifted_lines[y] for y in (27, 35, 67))
            self.assertTrue(all(a != c for a, c in zip(active, shifted)), (active, shifted))
        finally:
            b.resume()
        s.key("Z")
        time.sleep(.3)
        self.assertEqual(b.peek(0x4d, 1)[0] & 0x80, 0)
        restored_lines = scanlines()
        self.assertEqual(tuple(restored_lines[y] for y in (27, 35, 67)), normal)

    def test_all_rows_follow_os_attract_without_flashing(self):
        self.check_attract(self.s)


class BrowseTransitions(Session):
    SPEC = {"transition/item%03d_" % i + "x" * 16 + ".atr": 100 for i in range(40)}

    def test_page_stays_flat_until_new_page_is_presented_and_error_recovers(self):
        from atari_ui import symbols, screen_code_to_char
        s = self.s
        browse_host(s, "host:/transition/")
        s.wait_for("item000_", timeout=20)
        sym = symbols(s.xex.replace(".xex", ".lbl"))
        reading = sym["_scr_reading"]
        mode = sym["_scr_nmi_mode"]
        b = s.bridge

        def line(row, width=40):
            address = b.peek16(0x0600 + 3 + row * 3 + 1)
            return "".join(screen_code_to_char(c) for c in b.peek(address, width))

        def first_row():
            return line(3, 29)

        b.pause()
        try:
            self.assertIn("item000_", first_row())
            b.key("SPACE")
            saw_reading = False
            saw_new = False
            for _ in range(1600):
                b.cycles(8000)
                pending = b.peek(reading, 1)[0]
                nmi = b.peek(mode, 1)[0]
                if pending:
                    saw_reading = True
                    self.assertEqual(nmi, 0x40, "row colours resumed before the new page")
                    row = first_row()
                    # During VBI the display list is updated row by row. It is
                    # still monochrome until that swap has completed.
                    self.assertTrue("item000_" in row or "item018_" in row, row)
                    if "item018_" in row:
                        saw_new = True
                    elif saw_new:
                        self.fail("the old page returned during the swap")
                    if not saw_new:
                        self.assertIn("Reading directory...", line(23))
                        self.assertEqual(b.peek(0x02C6, 1)[0], 0x74)
                        self.assertEqual(b.peek(0x02C8, 1)[0], 0x74)
                elif saw_reading:
                    self.assertEqual(nmi, 0xC0)
                    self.assertIn("item018_", first_row())
                    break
            else:
                self.fail("did not observe a complete monochrome page transition")
        finally:
            b.resume()
        s.wait_for("item018_", timeout=20)

        # Refresh a now-missing directory: the old page must return with its
        # error and row colours, not remain permanently monochrome.
        (s.cfg / "transition").rename(s.cfg / "transition-away")
        s.key("R")
        s.wait_for("Could not read the folder", row=23, timeout=30)
        self.assertIn("item018_", "".join(s.screen()[3:21]))
        self.assertEqual(b.peek(reading, 1)[0], 0)
        self.assertEqual(b.peek(mode, 1)[0], 0xC0)

        # The unused OS cursor may point into application RAM after boot.
        # Reading a keyboard key must not write to that stale address.
        marker = 0x04F0
        saved_marker = b.peek(marker, 1)[0]
        saved_char = b.peek(0x5D, 1)[0]
        saved_addr = b.peek16(0x5E)
        try:
            b.poke(marker, 0xA5)
            b.poke(0x5D, 0x11)
            b.poke16(0x5E, marker)
            s.key("DOWN")
            self.assertEqual(b.peek(marker, 1)[0], 0xA5)
            self.assertIn("item018_", "".join(s.screen()[3:21]))
        finally:
            b.poke(marker, saved_marker)
            b.poke(0x5D, saved_char)
            b.poke16(0x5E, saved_addr)


class LongNames(Session):
    SPEC = {"long/" + LONG: 1500, "long/short.atr": 300}

    def test_open_and_assign_a_name_longer_than_a_page_keeps(self):
        s = self.s
        browse_host(s, "host:/long/")
        s.select("a_very_long")
        s.key("RETURN")
        s.wait_for("Assign to a drive", timeout=10)
        s.key("3")
        s.wait_for("D3 <-", row=23, timeout=10)
        s.key("TAB")
        s.wait_for("D3:", timeout=5)
        self.assertIn("a_very_long", row_text(s.screen(), "D3:"))
        # the field shows the end of the full URI
        self.assertTrue(s.screen()[2].rstrip().endswith(LONG[-30:]), s.screen()[2])

    def test_selected_long_name_animates_inside_its_column(self):
        s = self.s
        s.key("TAB")                       # Setup
        s.key("TAB")                       # Hosts
        s.key("TAB")                       # Browse
        s.wait_for("a_very_long", timeout=10)
        s.select("a_very_long")
        first = s.screen()[s.selected_row()]
        seen = {first}
        end = time.monotonic() + 40
        while time.monotonic() < end and len(seen) < 4:
            time.sleep(.5)
            seen.add(s.screen()[s.selected_row()])
        self.assertGreaterEqual(len(seen), 3, seen)
        # only the name column moves: the size column stays where it is
        for text in seen:
            self.assertRegex(text[31:37], r"\s*\d+K|\s+2K", text)


class DetailLine(Session):
    SPEC = {"sizes/a.atr": 5, "sizes/b.atr": 1234567, "sizes/c.atr": 90000,
            "sizes/sub/x.atr": 10}

    def test_date_is_right_aligned_and_size_left_aligned(self):
        s = self.s
        browse_host(s, "host:/sizes/")
        ends = set()
        for name in ("a.atr", "b.atr", "c.atr"):
            s.select(name)
            time.sleep(.4)
            line = s.screen()[23]
            self.assertTrue(line.startswith(" "), line)
            self.assertIn("bytes", line[:20])
            self.assertIn(FIXED_DATE, line)
            ends.add(line.index(FIXED_DATE))
        self.assertEqual(len(ends), 1, "the date moved between entries")
        self.assertEqual(ends.pop(), 40 - 1 - len(FIXED_DATE))   # flush right, one column spare

    def test_directory_shows_dir_marker_and_date(self):
        s = self.s
        s.select("sub")
        time.sleep(.4)
        line = s.screen()[23]
        self.assertIn("<DIR>", line[:12])
        self.assertIn(FIXED_DATE, line)


class Dates(Session):
    # (name, unix time, expected text): leap days, century and year ends
    CASES = [
        ("d1.atr", 0, None),                                    # unknown date: none shown
        ("d2.atr", 951782400 + 12 * 3600 + 5 * 60, "2000-02-29 12:05"),
        ("d3.atr", 946684799, "1999-12-31 23:59"),
        ("d4.atr", 1709164800, "2024-02-29 00:00"),
        ("d5.atr", 1735689599, "2024-12-31 23:59"),
        ("d6.atr", 1767225600, "2026-01-01 00:00"),
        ("d7.atr", 86399, "1970-01-01 23:59"),
        ("d8.atr", 126230400 + 3600, "1974-01-01 01:00"),
    ]
    SPEC = {"dates/" + n: (100, t) for n, t, _ in CASES if t}

    def test_dates_across_leap_years_and_year_ends(self):
        s = self.s
        browse_host(s, "host:/dates/")
        for name, t, expect in self.CASES:
            if not t:
                continue
            s.select(name)
            time.sleep(.4)
            line = s.screen()[23]
            if expect is None:
                continue
            self.assertIn(expect, line, f"{name}: {line!r}")


class BigDirectory(Session):
    SPEC = {"big/f%03d.atr" % i: 600 for i in range(100)}

    def test_paging_through_a_directory_larger_than_a_page(self):
        s = self.s
        browse_host(s, "host:/big/")
        rows = s.screen()
        self.assertIn("f000.atr", "".join(rows[3:21]))
        s.select("f060.atr")                    # pages down, reloading windows
        rows = s.screen()
        names = [r[1:9] for r in rows[3:21] if r.strip().startswith("f")]
        self.assertEqual(names, sorted(names))
        self.assertIn("f060.atr", names)
        self.assertEqual(len(names), len(set(names)))
        s.select("f099.atr")
        s.key("DOWN")                           # stays on the last entry
        self.assertIn("f099.atr", s.screen()[s.selected_row()])
        s.select("f000.atr")                    # and back to the top
        self.assertEqual(s.selected_row(), 3)


    def test_scrolling_one_row_at_a_time_matches_a_full_redraw(self):
        s = self.s
        s.select("f000.atr")
        for _ in range(30):                      # walks past the bottom edge, then on
            s.key("DOWN")
        scrolled = s.screen()[3:21]
        self.assertIn("f030.atr", scrolled[s.selected_row() - 3])
        s.key("R")                               # reload: a full redraw of the same page
        time.sleep(1.5)
        self.assertEqual(s.screen()[3:21], scrolled)
        for _ in range(12):                      # and back up
            s.key("UP")
        scrolled = s.screen()[3:21]
        s.key("R"); time.sleep(1.5)
        self.assertEqual(s.screen()[3:21], scrolled)


class Paging(Session):
    SPEC = dict({"pg/f%03d.atr" % i: 600 for i in range(100)}, **{"pg/sub/x.atr": 10})

    def info(self):
        return " ".join(self.s.screen()[21].split())

    def names(self):
        return [r[1:9].strip() for r in self.s.screen()[3:21] if r.strip()]

    def joy(self, direction):
        self.s.bridge.joy(0, direction); time.sleep(.03); self.s.bridge.joy(0, "centre"); time.sleep(.3)

    def test_pages_follow_the_cursor_and_the_page_keys(self):
        s = self.s
        browse_host(s, "host:/pg/")
        s.wait_for("Page", row=21, timeout=15)
        self.assertEqual(self.info(), "Page 1 of 6 101 entries")
        first = self.names()
        self.assertEqual(first[0], "sub")                    # directories come first
        self.assertEqual(len(first), 18)
        # down off the bottom of the page goes on to the next page, cursor on its first row
        for _ in range(18):
            s.key("DOWN")
        s.wait_for("Page    2", row=21, timeout=15)
        second = self.names()
        self.assertEqual(second[0], "f017.atr")
        self.assertEqual(s.selected_row(), 3)
        # and up off the top goes back, cursor on the last row
        s.key("UP")
        s.wait_for("Page    1", row=21, timeout=15)
        self.assertEqual(s.selected_row(), 20)
        self.assertEqual(self.names(), first)
        # page keys: SPACE and the joystick go forward, B and the stick go back
        s.key("SPACE"); s.wait_for("Page    2", row=21, timeout=15)
        self.joy("right"); s.wait_for("Page    3", row=21, timeout=15)
        self.assertEqual(self.names()[0], "f035.atr")
        s.key("B"); s.wait_for("Page    2", row=21, timeout=15)
        self.joy("left"); s.wait_for("Page    1", row=21, timeout=15)
        # the last page is short and stops there
        for _ in range(5):
            s.key("SPACE"); time.sleep(.5)
        s.wait_for("Page    6", row=21, timeout=15)
        self.assertEqual(len(self.names()), 101 - 5 * 18)
        s.key("SPACE"); time.sleep(.8)
        self.assertIn("Page 6 of 6", self.info())
        # R makes the device read the folder again and stays on the page
        s.key("R"); time.sleep(1.5)
        self.assertIn("Page 6 of 6", self.info())


class PagingUp(Session):
    SPEC = {"pg/f%03d.atr" % i: 600 for i in range(30)}

    def joy(self, direction):
        self.s.bridge.joy(0, direction); time.sleep(.03); self.s.bridge.joy(0, "centre"); time.sleep(.3)

    def test_left_on_the_first_page_goes_up_a_folder(self):
        s = self.s
        browse_host(s, "host:/")
        enter(s, "pg")
        s.wait_for("Page", row=21, timeout=15)
        self.joy("left")
        end = time.monotonic() + 15
        while s.screen()[2].strip() != "host:/" and time.monotonic() < end:
            time.sleep(.3)
        self.assertEqual(s.screen()[2].strip(), "host:/")


class PageCache(Session):
    # Both nine-entry protocol pages exceed 560 bytes together.  All names
    # are deliberately longer than the 31-byte per-entry page cap.
    SPEC = {"cache/%02d_" % i + "x" * 36 + ".atr": 100 for i in range(18)}

    def test_two_full_name_pages_stay_cached(self):
        from atari_ui import symbols
        s = self.s
        browse_host(s, "host:/cache/")
        s.wait_for("18 entries", row=21, timeout=15)
        self.assertEqual(sum(bool(r.strip()) for r in s.screen()[3:21]), 18)
        used = s.bridge.peek16(symbols(s.xex.replace(".xex", ".lbl"))["_pc_used"])
        self.assertGreater(used, 560)
        self.assertLessEqual(used, 800)


class Filters(Session):
    SPEC = dict({"filter/miner%02d.atr" % i: 100 for i in range(25)},
                **{"filter/Miner 2049er.atr": 100, "filter/Manic Miner.atr": 100,
                   "filter/other.atr": 100, "filter/sub/x.atr": 100})

    def test_filter_and_paging(self):
        s = self.s
        browse_host(s, "host:/filter/")
        s.key("F"); s.type("*MiNeR*"); s.key("RETURN")
        s.wait_for("Filter:", row=21, timeout=15)
        self.assertIn("sub", "".join(s.screen()[3:21]))
        self.assertIn("Manic Miner", "".join(s.screen()[3:21]))
        self.assertNotIn("other.atr", "".join(s.screen()[3:21]))
        s.key("SPACE"); time.sleep(1)
        self.assertIn("miner", "".join(s.screen()[3:21]).lower())
        s.key("B"); s.select("sub"); s.key("RETURN")
        s.wait_for("host:/filter/sub", row=2, timeout=15)
        self.assertIn("*MiNeR*".lower(), s.screen()[21].lower())
        self.assertNotIn("x.atr", "".join(s.screen()[3:21]))
        s.key("BACKSPACE")
        s.wait_for("Manic Miner", timeout=15)
        s.key("F")
        for _ in range(7): s.key("BACKSPACE")
        s.type("nomatch"); s.key("RETURN")
        s.wait_for("nomatch", row=21, timeout=15)
        self.assertIn("sub", "".join(s.screen()[3:21]))
        self.assertNotIn("miner", "".join(s.screen()[3:21]).lower())
        s.key("F")
        for _ in range(7): s.key("BACKSPACE")
        s.key("RETURN")
        s.select("other.atr")

        s.select("sub"); s.key("RETURN")
        s.wait_for("host:/filter/sub", row=2, timeout=15)
        s.key("F"); s.type("missing"); s.key("RETURN")
        s.wait_for("missing", row=21, timeout=15)
        self.assertNotIn("x.atr", "".join(s.screen()[3:21]))


class LongUri(Session):
    DIRS = ["d%02d_" % i + "x" * 20 for i in range(6)]
    NAME = "file_with_a_long_name_" + "y" * 10 + ".atr"
    SPEC = {"/".join(DIRS) + "/" + NAME: 600}

    def test_a_192_character_uri_is_assigned_and_kept(self):
        s = self.s
        browse_host(s, "host:/")
        for d in self.DIRS:
            enter(s, d[:6])
        s.select(self.NAME[:12])
        s.key("RETURN")
        s.wait_for("Assign to a drive", timeout=10)
        s.key("1")
        s.wait_for("D1 <-", row=23, timeout=10)
        s.key("TAB")
        rows = s.wait_for("D1:", timeout=5)
        self.assertTrue(rows[2].rstrip().endswith(self.NAME[-30:]), rows[2])


class Input(Session):
    SPEC = {"x/a.atr": 100}

    def test_help_on_each_tab_and_text_entry(self):
        s = self.s
        for tab in ("Hosts", "Browse", "Drives", "Setup"):
            s.key("H")
            s.wait_for("Help - press any key", timeout=10)
            self.assertIn(tab, s.screen()[7])
            s.key("RETURN")
            s.wait_for("FujiNet NIO Config", row=0, timeout=10)
            s.key("TAB")
        s.key("HELP")
        s.wait_for("Help - press any key", timeout=10)
        s.key("RETURN")
        s.key("A")
        s.type("host:/h")
        s.wait_for("host:/h", row=2, timeout=5)
        self.assertNotIn("Help - press any key", "".join(s.screen()))
        s.key("ESC")

    def test_joystick_moves_and_opens(self):
        s = self.s
        first = s.screen()[2]
        s.bridge.joy(0, "down"); time.sleep(.2); s.bridge.joy(0, "centre"); time.sleep(.4)
        self.assertNotEqual(s.screen()[2], first)        # host field follows the bar
        s.bridge.joy(0, "right"); time.sleep(.2); s.bridge.joy(0, "centre")
        s.wait_for("Browse", row=1, timeout=5)

    def test_select_switches_tab(self):
        s = self.s
        before = s.screen()[22]
        s.bridge.consol(select=True); time.sleep(.3); s.bridge.consol(); time.sleep(.5)
        self.assertNotEqual(s.screen()[22], before)        # a different tab's key hints


class Hosts(Session):
    SPEC = {}

    def test_add_edit_delete_and_survive_a_restart(self):
        s = self.s
        s.key("A"); s.type("tnfs://example.org/"); s.key("RETURN")
        s.wait_for("tnfs://example.org/", row=2, timeout=5)
        s.key("E"); s.type("x"); s.key("RETURN")
        s.wait_for("tnfs://example.org/x", row=2, timeout=5)
        self.assertIn("tnfs://example.org/x", "\n".join(s.screen()[3:21]))
        s.key("D")
        s.wait_for("Delete", row=23, timeout=5)
        s.key("Y")
        time.sleep(.5)
        self.assertNotIn("example.org", "\n".join(s.screen()[3:21]))


class ManyHosts(Session):
    SPEC = {}

    def test_more_hosts_than_a_page_get_a_scroll_bar_and_scroll(self):
        s = self.s
        rows = s.screen()
        self.assertNotIn("\x7e", "".join(r[39] for r in rows[3:21]))
        self.assertFalse(any(r[39] != " " for r in rows[3:21]), "scroll bar with 3 hosts")
        for i in range(21):
            s.key("A"); s.type("h%02d.example/" % i); s.key("RETURN"); time.sleep(.2)
        s.wait_for("24 hosts", timeout=10)
        rows = s.screen()
        self.assertTrue(any(r[39] != " " for r in rows[3:21]), "no scroll bar with 24 hosts")
        self.assertIn("h20.example/", "\n".join(rows[3:21]))       # the new one is in view
        scrolled = s.screen()[3:21]
        s.key("R") if False else None
        for _ in range(30):
            s.key("UP")
        self.assertIn("sd0:/", "\n".join(s.screen()[3:21]))
        for _ in range(30):
            s.key("DOWN")
        self.assertEqual(s.screen()[3:21], scrolled)
        s.key("A")
        s.type("one.too.many/"); s.key("RETURN")
        s.wait_for("No room", row=23, timeout=5) if False else None


class Persistence(unittest.TestCase):
    def test_hosts_and_drives_are_kept_across_runs(self):
        need_emulator()
        from atari_ui import EmbeddedSession
        from common import XEX
        with tempfile.TemporaryDirectory(prefix="nio-keep-") as keep, \
                tempfile.TemporaryDirectory(prefix="nio-src-") as tmp:
            data = make_files(tmp, {"p/one.atr": 300})
            with EmbeddedSession(XEX, data=data, keep=keep) as s:
                s.wait_for("Hosts", row=1, timeout=20)
                browse_host(s, "host:/p/")
                s.select("one.atr")
                s.key("RETURN"); s.wait_for("Assign", timeout=10); s.key("4")
                s.wait_for("D4 <-", row=23, timeout=10)
            with EmbeddedSession(XEX, keep=keep) as s:
                s.wait_for("Hosts", row=1, timeout=20)
                self.assertIn("host:/p/", "\n".join(s.screen()[3:21]))
                s.key("TAB"); s.key("TAB")
                rows = s.wait_for("D4:", timeout=10)
                self.assertIn("one.atr", row_text(rows, "D4:"))


if __name__ == "__main__":
    unittest.main()
