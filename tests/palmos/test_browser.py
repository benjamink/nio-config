"""Execute Palm browser code and NioLib against mocked OS traps/wire peers.
No service implementation, parser, or application operation is replaced.
"""
import struct
import re
import unittest
from test_config import ConfigMachine, OUT, foundation
from test_palmos_shared_network import reply
from unicorn.m68k_const import UC_M68K_REG_A0, UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_PC


def install_state(total):
    # Palm's two-byte alignment; private Install state includes progress timing.
    return struct.pack('>IIHH4BIIH', 0, total, 0, 0, 0, 0, 0, 0, 0, 0, 0)


class BrowserMachine(ConfigMachine):
    def __init__(self):
        self.host_record = None
        self.directory = [(0, f'File{i:03d}.prc'.encode(), 123) for i in range(125)]
        self.bad_page = False
        self.fail_device = None
        self.bad_offset = False
        self.short_write = False
        self.page_requests = []
        self.files = {}
        self.active_form = 1500
        self.fields = {1501: b'new-network', 1502: b'trial-password'}
        self.alerts = []
        self.alert_ids, self.alert_results = [], {}
        self.selections, self.control_labels, self.visible, self.enabled = {}, {}, {}, {}
        self.field_handles = {}
        self.wifi_ssid = b'Current AP'
        self.wifi_state, self.wifi_config_error, self.wifi_status_error = 2, 0, 0
        self.join_state = 1
        self.join_requests = []
        self.old_service_api = False
        self.fail_field_alloc = False
        self.scrollbars = {}
        self.list_tops = {}
        self.read_failures = 0
        self.read_failure_code = 5
        self.read_limit = None
        self.read_ticks = 0
        self.eof_read_error = False
        self.empty_without_eof = False
        self.force_read_recovery = False
        self.packet_limit = None
        self.status_return = None
        self.drawn_text = []
        self.drawn_positions = []
        self.auto_off_resets = 0
        self.meter_fills, self.meter_frames, self.window_events = [], [], []
        self.fail_popup_alloc = False
        self.previous_form = None
        super().__init__()

    def string(self, address):
        data = bytearray()
        while self.uc.mem_read(address + len(data), 1)[0]:
            data += self.uc.mem_read(address + len(data), 1)
        return bytes(data)

    def hook(self, uc, pc, size, context):
        if self.status_return and pc == self.status_return[0]:
            # Adjust only the advertised limit after the real library trap
            # returns, preserving its status fields and ownership behavior.
            uc.mem_write(self.status_return[1] + 8, struct.pack('>H', self.packet_limit))
            self.status_return = None
        if bytes(uc.mem_read(pc, 2)) != b'\x4e\x4f':
            return super().hook(uc, pc, size, context)
        trap, sp = self.read16(pc + 2), uc.reg_read(UC_M68K_REG_A7)
        result = 0
        if self.old_service_api and trap in (0xa805, 0xa806) and self.read16(sp) == 7:
            data = struct.pack('>4H', 8, 1, 5, 8) if trap == 0xa805 else struct.pack('>I', 0x800000ff)
            uc.mem_write(self.read32(sp + 2), data)
        elif trap == 0xa815 and self.force_read_recovery:
            uc.mem_write(self.read32(sp + 2), struct.pack('>8H', 16, 1, 1, 8, 1024, 4, 0, 0))
        elif trap == 0xa815 and self.packet_limit is not None:
            self.status_return = (pc + 4, self.read32(sp + 2))
            super().hook(uc, pc, size, context)
            return
        elif trap == 0xa2b6:
            self.scrollbars[self.read32(sp)] = struct.unpack('>4h', uc.mem_read(sp + 4, 8))
        elif trap == 0xa2b5: self.list_tops[self.read32(sp)] = self.read16(sp + 4)
        elif trap == 0xa1b3: result = self.selections.get(self.read32(sp), 0xffff)
        elif trap == 0xa1b7: self.selections[self.read32(sp)] = self.read16(sp + 4)
        elif trap == 0xa1b4:
            choices = {1013: [b'9600', b'19200', b'38400', b'57600', b'115200'],
                       1014: [b'None', b'RTS/CTS']}
            ident, selected = self.read32(sp), self.read16(sp + 4)
            pointer = 0x88000 + (ident - 1013) * 256 + selected * 32
            uc.mem_write(pointer, choices[ident][selected] + b'\0')
            uc.reg_write(UC_M68K_REG_A0, pointer)
        elif trap == 0xa114: self.control_labels[self.read32(sp)] = self.string(self.read32(sp + 4))
        elif trap == 0xa117: self.enabled[self.read32(sp)] = bool(uc.mem_read(sp + 4, 1)[0])
        elif trap in (0xa184, 0xa185): self.visible[self.read16(sp + 4)] = trap == 0xa185
        elif trap == 0xa2d9: result = min(30, len(self.string(self.read32(sp))))
        elif trap in (0xa171, 0xa112, 0xa1b1, 0xa1b8, 0xa2b7): pass
        elif trap == 0xa2d6:
            target, source, length = self.read32(sp), self.read32(sp + 4), self.read16(sp + 8)
            uc.mem_write(target, (self.string(target) + self.string(source))[:length-1] + b'\0')
            uc.reg_write(UC_M68K_REG_A0, target)
        elif trap == 0xa0c9:
            target, value = self.read32(sp), self.read32(sp + 4)
            uc.mem_write(target, str(value).encode() + b'\0')
            uc.reg_write(UC_M68K_REG_A0, target)
        elif trap == 0xa01e:
            pointer = 0 if self.fail_field_alloc else 0x8a000 + len(self.field_handles) * 256
            if pointer: self.field_handles[pointer] = self.read32(sp)
            uc.reg_write(UC_M68K_REG_A0, pointer)
        elif trap == 0xa021 and self.read32(sp) in self.field_handles:
            uc.reg_write(UC_M68K_REG_A0, self.read32(sp))
        elif trap == 0xa022 and self.read32(sp) in self.field_handles: pass
        elif trap == 0xa19b: self.active_form = self.read16(sp)
        elif trap == 0xa16f:
            self.previous_form = self.active_form
            self.window_events.append('create')
            uc.reg_write(UC_M68K_REG_A0, 0 if self.fail_popup_alloc else 0x81100)
        elif trap == 0xa174:
            self.active_form = 1600 if self.read32(sp) == 0x81100 else self.previous_form
            self.window_events.append(('active', self.active_form))
        elif trap in (0xa172, 0xa170):
            self.window_events.append('erase' if trap == 0xa172 else 'delete')
        elif trap == 0xa342:
            self.assert_popup_active = self.active_form
            result = 1  # An OS install failure must still restore the browser.
        elif trap == 0xa173:
            uc.reg_write(UC_M68K_REG_A0, 0x81100 if self.active_form == 1600 else 0x81000)
        elif trap == 0xa17d: result = self.active_form
        elif trap == 0xa180: result = self.read16(sp + 4)
        elif trap == 0xa183: uc.reg_write(UC_M68K_REG_A0, self.read16(sp + 4))
        elif trap == 0xa139:
            ident = self.read32(sp)
            pointer = 0x82000 + (ident - 1500) * 256
            uc.mem_write(pointer, self.fields[ident] + b'\0')
            uc.reg_write(UC_M68K_REG_A0, pointer)
        elif trap == 0xa14b: result = len(self.fields[self.read32(sp)])
        elif trap == 0xa158: self.fields[self.read32(sp)] = self.string(self.read32(sp + 4))
        elif trap == 0xa15d:
            self.fields[self.read32(sp)] += bytes(uc.mem_read(self.read32(sp + 4), self.read16(sp + 8)))
            result = 1
        elif trap == 0xa142: pass
        elif trap == 0xa15e: self.fields[self.read32(sp)] = b''
        elif trap == 0xa220:
            self.drawn_text.append((self.now, bytes(uc.mem_read(self.read32(sp), self.read16(sp + 4)))))
            self.drawn_positions.append((self.drawn_text[-1][1], self.read16(sp + 6), self.read16(sp + 8)))
        elif trap == 0xa130: self.auto_off_resets += 1
        elif trap in (0xa218, 0xa21b):
            rectangle = self.read32(sp if trap == 0xa218 else sp + 2)
            coords = struct.unpack('>4h', uc.mem_read(rectangle, 8))
            (self.meter_fills if trap == 0xa218 else self.meter_frames).append(coords)
        elif trap in (0xa135, 0xa219): pass
        elif trap == 0xa16d:
            uc.mem_write(self.read32(sp + 12), b'\x01')
        elif trap == 0xa194:
            self.alerts.append(self.string(self.read32(sp + 2)))
            self.alert_ids.append(self.read16(sp))
            result = self.alert_results.get(self.read16(sp), 0)
        elif trap == 0xa2de:  # Palm StrPrintF: 16-bit int, 32-bit long varargs
            target = self.read32(sp)
            format_string = self.string(self.read32(sp + 4)).decode()
            offset = sp + 8
            def substitute(match):
                nonlocal offset
                spec = match[0]
                if spec.endswith('s'):
                    value = self.string(self.read32(offset)).decode(); offset += 4
                else:
                    long = 'l' in spec
                    value = self.read32(offset) if long else self.read16(offset)
                    offset += 4 if long else 2
                    if spec.endswith('d') and value & (1 << (31 if long else 15)):
                        value -= 1 << (32 if long else 16)
                return spec.replace('l', '') % value
            output = re.sub(r'%0?\d*l?[sudx]', substitute, format_string).encode()
            uc.mem_write(target, output + b'\0'); result = len(output)
        elif trap == 0xa0c6:  # StrCat
            target = self.read32(sp)
            uc.mem_write(target, self.string(target) + self.string(self.read32(sp + 4)) + b'\0')
            uc.reg_write(UC_M68K_REG_A0, target)
        elif trap in (0xa0c8, 0xa0ca):
            a, b = self.string(self.read32(sp)), self.string(self.read32(sp + 4))
            if trap == 0xa0ca: a, b = a.lower(), b.lower()
            result = 0 if a == b else 1 if a > b else 0xffff
        elif trap == 0xa0cc:
            pointer, value = self.read32(sp), self.read16(sp + 4) & 255
            index = (self.string(pointer) + b'\0').find(bytes([value]))
            uc.reg_write(UC_M68K_REG_A0, pointer + index if index >= 0 else 0)
        else:
            return super().hook(uc, pc, size, context)
        uc.reg_write(UC_M68K_REG_D0, result)
        uc.reg_write(UC_M68K_REG_PC, pc + 4)

    def execute(self, request):
        dev, cmd = request[:2]
        if dev not in (0xf1, 0xfe, 0xf3, 0x70): return super().execute(request)
        self.requests.append(request)
        if dev == self.fail_device: return reply(dev, cmd, b'', 5)
        payload = request[6:]
        if dev == 0x70: return reply(dev, cmd, b"\x01\x03v1!\x04test")
        def text(at):
            length = int.from_bytes(payload[at:at+2], 'little')
            return payload[at+2:at+2+length], at+2+length
        if dev == 0xf3:
            if cmd == 2:
                if self.wifi_config_error: return reply(dev, cmd, b'', self.wifi_config_error)
                return reply(dev, cmd, bytes([1, 1, 1, len(self.wifi_ssid)]) + self.wifi_ssid + b'\0')
            if cmd == 1:
                if self.wifi_status_error: return reply(dev, cmd, b'', self.wifi_status_error)
                return reply(dev, cmd, bytes([1, self.wifi_state, 1, 0, 1, 200]) + bytes(6) + bytes(4))
            if cmd == 4:
                out = b'\x01\0\x02'
                for name in (b'Scanned AP', b'Other AP'):
                    out += bytes([len(name)]) + name + bytes(6) + b'\xc8\x01\x03'
                return reply(dev, cmd, out)
            if cmd == 6:
                ssid_len = payload[1]
                ssid = payload[2:2+ssid_len]
                at = 2+ssid_len
                password = payload[at+1:at+1+payload[at]]
                self.join_requests.append((ssid, password))
                return reply(dev, cmd, b'\x01')
            if cmd == 7: return reply(dev, cmd, bytes([1, self.join_state]))
            raise AssertionError(cmd)
        name, at = text(1)
        if dev == 0xf1:
            key, at = text(at)
            assert (name, key) == (b'config-nio', b'hosts')
            offset, count = struct.unpack_from('<IH', payload, at)
            if cmd == 2:
                data = (self.host_record or b'')[offset:offset+count]
                flags = (2 if self.host_record is not None else 0)
                if offset + len(data) >= len(self.host_record or b''): flags |= 1
                out = struct.pack('<BBHIH', 1, flags, 0, offset + self.bad_offset, len(data)) + data
            elif cmd == 3:
                data = payload[at+6:at+6+count]
                previous = (self.host_record or b'') if offset else b''
                self.host_record = previous[:offset] + data + previous[offset+count:]
                out = struct.pack('<BBHIH', 1, 0, 0, offset, len(data) - self.short_write)
            else: raise AssertionError(cmd)
        else:
            if cmd == 1:
                return reply(dev, cmd, struct.pack('<BBHIIII', 1, 2, 0,
                             len(self.files.get(name, b'')), 0, 0, 0))
            if cmd == 3:
                offset, count = struct.unpack_from('<IH', payload, at)
                if self.read_limit is not None and count > self.read_limit:
                    return reply(dev, cmd, b'', 5)
                if self.read_failures:
                    self.read_failures -= 1
                    return reply(dev, cmd, b'', self.read_failure_code)
                if self.eof_read_error and offset >= len(self.files.get(name, b'')):
                    return reply(dev, cmd, b'', 5)
                if self.empty_without_eof:
                    return reply(dev, cmd, struct.pack('<BBHIH', 1, 0, 0, offset, 0))
                data = self.files.get(name, b'')[offset:offset+count]
                self.now = (self.now + self.read_ticks) & 0xffffffff
                # FileDevice marks a full read TRUNCATED, even at exact EOF.
                flags = 1 if len(data) < count else 2
                out = struct.pack('<BBHIH', 1, flags, 0, offset + self.bad_offset, len(data)) + data
                return reply(dev, cmd, out)
            assert cmd == 7
            start, page_size, budget, flags, max_name = struct.unpack_from('<HBHBB', payload, at)
            self.page_requests.append((name, start, budget, flags))
            assert page_size == 1
            pages = b''
            count = 0
            for entry_flags, filename, size in self.directory[start:]:
                shortened = filename[:max_name]
                if len(shortened) < len(filename): entry_flags |= 0x80
                entry = bytes([entry_flags, len(shortened)]) + shortened + struct.pack('<II', size, 0)
                page = bytes([1]) + struct.pack('<H', len(entry)) + entry
                if len(pages) + len(page) > budget: break
                pages += page
                count += 1
            more = int(start + count < len(self.directory))
            out = struct.pack('<BBHIHBB', 1, more, 0, len(self.directory), start, 1, count) + pages
            if self.bad_page: out = out[:-1]
        return reply(dev, cmd, out)

    def parse_hosts(self, text):
        self.uc.mem_write(0x90000, text + b'\0')
        return self.app('palm_parse_hosts', struct.pack('>IH', 0x90000, len(text))) & 255

    def compose(self, host, path=b'', name=b'', capacity=1024):
        for address, value in zip((0x90000, 0x91000, 0x92000), (host, path, name)):
            self.uc.mem_write(address, value + b'\0')
        ok = self.app('palm_compose_uri', struct.pack('>IIIIH', 0x90000, 0x91000, 0x92000, 0x93000, capacity)) & 255
        return ok, self.string(0x93000) if ok else b''


class BrowserTests(unittest.TestCase):
    def test_palm_resources_and_no_embedded_services(self):
        _, resources = foundation.image.prc((OUT/'NioConfig.prc').read_bytes())
        for form in (1000, 1100, 1200, 1300, 1400, 1500, 1600): self.assertIn((b'tFRM', form), resources)
        self.assertEqual({ident for kind, ident in resources if kind == b'Talt'}, {9100, 9101, 9102, 9103})
        symbols = (OUT/'NioConfig.symbols').read_text()
        for name in ('fn_raw_call', 'fn_file_read', 'fn_file_list_pages', 'fn_appstore_write'):
            self.assertNotIn(' T ' + name + '\n', symbols)

    def test_uri_roots_and_bounded_output(self):
        m = BrowserMachine()
        for host, path, leaf, expected in (
            (b'host:16384', b'PALM/', b'App.prc', b'tnfs://host:16384/PALM/App.prc'),
            (b'tnfs://host/PALM/', b'Apps/', b'', b'tnfs://host/PALM/Apps/'),
            (b'sd0:/', b'', b'App.prc', b'sd0:/App.prc'),
            (b'persist:///hotsync/', b'', b'', b'persist:///hotsync/')):
            self.assertEqual(m.compose(host, path, leaf), (1, expected))
        self.assertFalse(m.compose(b'host', b'x' * 100, capacity=20)[0])
        self.assertFalse(m.compose(b'')[0])

    def test_host_record_validation_is_atomic(self):
        m = BrowserMachine()
        self.assertTrue(m.parse_hosts(b' host \r\nsd0:/\n\n'))
        self.assertEqual(m.word('palmHostCount'), 2)
        original = bytes(m.uc.mem_read(m.variable('palmHosts'), 8*241))
        for invalid in (b'a\n' * 9, b'x' * 241, b'good\nbad\0host'):
            self.assertFalse(m.parse_hosts(invalid))
            self.assertEqual(bytes(m.uc.mem_read(m.variable('palmHosts'), 8*241)), original)

    def test_shared_hosts_load_save_and_empty_record(self):
        m = BrowserMachine(); m.app('config_start')
        m.host_record = b'tnfs://one/\nsd0:/\n'
        self.assertTrue(m.app('palm_hosts_load'))
        self.assertEqual(m.word('palmHostCount'), 2)
        self.assertTrue(m.parse_hosts(b'apps.irata.online\npersist:///\n'))
        self.assertTrue(m.app('palm_hosts_save'))
        self.assertEqual(m.host_record, b'apps.irata.online\npersist:///\n')
        self.assertTrue(m.parse_hosts(b'')); self.assertTrue(m.app('palm_hosts_save'))
        self.assertEqual(m.host_record, b'')
        self.assertFalse(m.byte('clientOpen')); m.clean()

    def test_host_chunks_long_records_and_write_errors(self):
        m = BrowserMachine(); m.app('config_start')
        m.host_record = b'\n'.join([b'x' * 240] * 8) + b'\n'
        self.assertTrue(m.app('palm_hosts_load'))
        self.assertEqual(m.word('palmHostCount'), 8)
        self.assertTrue(m.app('palm_hosts_save'))
        self.assertEqual(m.host_record, b'\n'.join([b'x' * 240] * 8) + b'\n')
        m.short_write = True
        self.assertFalse(m.app('palm_hosts_save'))
        self.assertFalse(m.byte('clientOpen')); m.clean()

    def test_missing_hosts_seed_without_implicit_write(self):
        m = BrowserMachine(); m.app('config_start')
        self.assertTrue(m.app('palm_hosts_load'))
        self.assertEqual(m.word('palmHostCount'), 3)
        self.assertIsNone(m.host_record)
        m.clean()

    def test_old_library_and_unsaved_transport_never_send_services(self):
        m = BrowserMachine(); m.incompatible = True; m.app('config_start')
        self.assertFalse(m.app('palm_hosts_load')); self.assertFalse(m.requests)
        m = BrowserMachine(); m.app('config_start'); m.action(1002)
        self.assertFalse(m.app('palm_hosts_load')); self.assertFalse(m.requests); m.clean()
        m = BrowserMachine(); m.app('config_start')
        m.uc.mem_write(m.variable('servicesSupported'), b'\0')
        self.assertFalse(m.app('palm_hosts_load')); self.assertFalse(m.requests); m.clean()

    def test_host_error_and_wrong_offset_do_not_publish_partial_record(self):
        for bad in ('fail_device', 'bad_offset'):
            m = BrowserMachine(); m.app('config_start')
            self.assertTrue(m.parse_hosts(b'keep\n'))
            m.host_record = b'replacement\n'
            setattr(m, bad, 0xf1 if bad == 'fail_device' else True)
            self.assertFalse(m.app('palm_hosts_load'))
            self.assertEqual(m.string(m.variable('palmHosts')), b'keep')
            self.assertFalse(m.byte('clientOpen')); m.clean()

    def test_browse_more_than_100_files_on_each_transport(self):
        for transport in (1, 2, 3):
            m = BrowserMachine(); m.app('config_start')
            if transport != 1: m.action(1002 if transport == 2 else 1007); m.app('config_save')
            self.assertTrue(m.parse_hosts(b'host\n'))
            seen = []
            for start in range(0, 125, 8):
                m.uc.mem_write(m.variable('palmBrowseStart'), struct.pack('>H', start))
                self.assertTrue(m.app('palm_browse_load', b'\0\0'))
                self.assertEqual(m.read32(m.variable('palmBrowseTotal')), 125)
                for row in range(m.word('palmEntryCount')):
                    seen.append(m.string(m.variable('palmEntries') + row * 228))
                self.assertFalse(m.byte('clientOpen'))
            self.assertEqual(seen, [entry[1] for entry in m.directory])
            m.clean()

    def test_long_and_cut_names_and_refresh(self):
        m = BrowserMachine(); m.app('config_start'); self.assertTrue(m.parse_hosts(b'host\n'))
        m.directory = [(1, b'folder', 0)] + [(0, b'x' * 220, 1)] * 7 + [(0, b'y' * 225, 1)]
        self.assertTrue(m.app('palm_browse_load', b'\x01\0'))
        self.assertEqual(m.word('palmEntryCount'), 8)
        self.assertEqual(len(m.page_requests), 4)
        self.assertTrue(m.page_requests[0][3] & 4)
        self.assertTrue(all(not request[3] & 4 for request in m.page_requests[1:]))
        m.uc.mem_write(m.variable('palmBrowseStart'), b'\0\x08')
        self.assertTrue(m.app('palm_browse_load', b'\0\0'))
        self.assertTrue(m.uc.mem_read(m.variable('palmEntries') + 226, 1)[0] & 0x80)
        m.clean()

    def test_malformed_directory_and_unsafe_names_clear_rows_and_release_owner(self):
        for bad in ('truncated', 'slash', 'parent', 'nul', 'empty'):
            m = BrowserMachine(); m.app('config_start'); self.assertTrue(m.parse_hosts(b'host\n'))
            if bad == 'truncated': m.bad_page = True
            else: m.directory = [(0, {'slash': b'a/b', 'parent': b'..', 'nul': b'a\0b', 'empty': b''}[bad], 1)]
            self.assertFalse(m.app('palm_browse_load', b'\0\0'))
            self.assertEqual(m.word('palmEntryCount'), 0)
            self.assertFalse(m.byte('clientOpen')); m.clean()

    def test_abi_15_keeps_setup_and_clock_but_rejects_browser(self):
        m = BrowserMachine(); m.old_service_api = True; m.app('config_start')
        self.assertTrue(m.byte('libraryOpen')); self.assertFalse(m.byte('servicesSupported'))
        self.assertFalse(m.app('palm_hosts_load')); self.assertFalse(m.requests)
        m.app('config_test'); self.assertIn('PASS', m.text()); m.clean()

    def test_install_stream_buffers_small_callback_reads_and_detects_bad_offsets(self):
        for bad_offset in (False, True):
            m = BrowserMachine(); m.app('config_start')
            uri, data = b'tnfs://host/App.prc', bytes(range(256)) * 3
            m.files[uri] = data; m.bad_offset = bad_offset
            m.uc.mem_write(m.variable('currentUri'), uri + b'\0')
            self.assertTrue(m.app('begin'))
            m.uc.mem_write(0x94000, install_state(len(data)))
            received = b''
            while len(received) < len(data):
                want = min(17, len(data)-len(received))
                m.uc.mem_write(0x96000, struct.pack('>I', want))
                error = m.app('install_read', struct.pack('>III', 0x95000, 0x96000, 0x94000)) & 0xffff
                if bad_offset:
                    self.assertNotEqual(error, 0)
                    self.assertEqual(m.read32(0x96000), 0)
                    break
                self.assertEqual(error, 0)
                count = m.read32(0x96000)
                self.assertEqual(count, want)
                received += bytes(m.uc.mem_read(0x95000, count))
            if not bad_offset:
                self.assertEqual(received, data)
                reads = [r for r in m.requests if r[:2] == b'\xfe\x03']
                self.assertEqual(len(reads), 3)
            m.app('config_end_services'); self.assertFalse(m.byte('clientOpen')); m.clean()

    def test_wifi_trial_erases_password_and_polls_without_holding_session(self):
        m = BrowserMachine(); m.app('config_start'); m.app('wifi_join')
        self.assertEqual(m.join_requests, [(b'new-network', b'trial-password')])
        self.assertEqual(m.fields[1502], b'')
        self.assertTrue(m.app('palm_join_pending')); self.assertFalse(m.byte('clientOpen'))
        m.action(1002)  # Do not change backend while polling a pending trial.
        self.assertEqual(m.draft()[4:6], b'\0\x01')
        m.join_state = 2; m.now += 100; m.app('palm_page_tick')
        self.assertFalse(m.app('palm_join_pending')); self.assertFalse(m.byte('clientOpen'))
        self.assertIn(b'Wi-Fi connected.', m.alerts); m.clean()

    def test_wifi_join_failure_and_unknown_result_are_reported_without_replay(self):
        for state in (3, None):
            m = BrowserMachine(); m.app('config_start'); m.app('wifi_join')
            if state is not None: m.join_state = state
            m.now += 100 if state is not None else 6000
            m.app('palm_page_tick')
            self.assertFalse(m.app('palm_join_pending'))
            self.assertEqual(len(m.join_requests), 1)
            self.assertTrue(any(b'restoring' in a or b'unavailable' in a for a in m.alerts))
            m.clean()

    def test_accepted_wifi_join_survives_cleanup_failure_without_resubmission(self):
        for error in ('end_error', 'end_result'):
            with self.subTest(error=error):
                m = BrowserMachine(); m.app('config_start')
                setattr(m, error, 0x8006 if error == 'end_error' else 6)
                m.app('wifi_join')
                self.assertTrue(m.byte('joinPending'))
                self.assertEqual(len(m.join_requests), 1)
                self.assertEqual(m.fields[1502], b'')
                self.assertIn(b'accepted; cleanup failed', m.alerts[-1])
                token = m.read32(m.variable('clientToken'))
                m.action(1002)
                self.assertEqual(m.draft()[4:6], b'\0\x01')
                m.app('wifi_join')
                self.assertEqual(len(m.join_requests), 1)
                alerts = len(m.alerts)
                m.now += 100; m.app('palm_page_tick')
                self.assertTrue(m.byte('joinPending'))
                self.assertEqual(len(m.alerts), alerts)
                if error == 'end_error':
                    self.assertEqual(m.read32(m.variable('clientToken')), token)
                    m.end_error = 0
                    m.app('config_end_services')
                else: m.end_result = 0
                m.join_state = 2; m.now += 100; m.app('palm_page_tick')
                self.assertFalse(m.byte('joinPending'))
                self.assertEqual(len(m.join_requests), 1)
                self.assertIn(b'Wi-Fi connected.', m.alerts)
                m.clean()

    def test_wifi_poll_failure_keeps_trial_pending_until_result_or_deadline(self):
        for complete in (False, True):
            with self.subTest(complete=complete):
                m = BrowserMachine(); m.app('config_start'); m.app('wifi_join')
                m.fail_device = 0xf3
                for _ in range(2):
                    m.now += 100; m.app('palm_page_tick')
                    self.assertTrue(m.byte('joinPending'))
                self.assertEqual(len(m.alerts), 2)  # Submission and one poll error.
                m.action(1002)
                self.assertEqual(m.draft()[4:6], b'\0\x01')
                m.fail_device = None
                if complete: m.join_state = 3; m.now += 100
                else: m.now += 6000
                m.app('palm_page_tick')
                self.assertFalse(m.byte('joinPending'))
                self.assertEqual(len(m.join_requests), 1)
                self.assertIn(b'restoring' if complete else b'unavailable', m.alerts[-1])
                m.clean()

    def test_terminal_wifi_status_is_kept_even_if_poll_cleanup_fails(self):
        m = BrowserMachine(); m.app('config_start'); m.app('wifi_join')
        m.join_state = 2; m.end_result = 6; m.now += 100
        m.app('palm_page_tick')
        self.assertFalse(m.byte('joinPending'))
        self.assertIn(b'Cannot check Wi-Fi join', m.alerts[-1])
        m.end_result = 0; m.clean()

    def test_failed_host_removal_restores_model_and_requires_reload(self):
        m = BrowserMachine(); m.app('config_start')
        m.host_record = b'alpha\nbeta\ngamma\n'
        self.assertTrue(m.app('palm_hosts_load'))
        m.active_form = 1400; m.fields[1401] = b''; m.fail_device = 0xf1
        m.uc.mem_write(m.variable('editSlot'), b'\0\x01')
        m.uc.mem_write(0x98000, struct.pack('>HBBhhH', 9, 0, 0, 0, 0, 1402) + bytes(16))
        m.app('palm_page_event', struct.pack('>I', 0x98000))
        self.assertEqual(m.word('palmHostCount'), 3)
        self.assertEqual([m.string(m.variable('palmHosts') + i*241) for i in range(3)],
                         [b'alpha', b'beta', b'gamma'])
        self.assertFalse(m.byte('hostsLoaded')); self.assertFalse(m.byte('clientOpen'))
        self.assertEqual(m.active_form, 1100); m.clean()

    def test_field_allocation_failure_cannot_turn_an_edit_into_deletion(self):
        m = BrowserMachine(); m.app('config_start')
        m.active_form = 1400; m.fail_field_alloc = True
        m.uc.mem_write(0x98000, struct.pack('>H', 24) + bytes(24))
        m.app('palm_page_event', struct.pack('>I', 0x98000))
        self.assertEqual(m.active_form, 1100)
        self.assertIn(b'Not enough memory for this field.', m.alerts)
        self.assertFalse(m.requests); m.clean()

    def test_setup_status_is_passive_and_preserves_draft(self):
        m = BrowserMachine(); m.app('config_start'); m.action(1002)
        draft = m.draft(); m.app('status_read')
        self.assertEqual(m.draft(), draft); self.assertTrue(m.byte('dirty'))
        self.assertFalse(m.requests); self.assertFalse(m.serial_calls); m.clean()

    def event(self, machine, kind, ident=0, value=0):
        machine.uc.mem_write(0x99000, struct.pack('>HBBhhHIhh', kind, 0, 0, 0, 0, ident, 0, 0, value) + bytes(8))
        return machine.app('palm_page_event', struct.pack('>I', 0x99000))

    def tap(self, machine, ident, selection, count):
        machine.uc.mem_write(0x99000, struct.pack('>HBBhh', 1, 1, count, 10, 50) + bytes(24))
        machine.app('palm_page_event', struct.pack('>I', 0x99000))
        # Native-generated list events need not carry the pen's tapCount.
        machine.uc.mem_write(0x99000, struct.pack('>HBBhhHIh', 12, 0, 0, 10, 50, ident, 0, selection) + bytes(16))
        return machine.app('palm_page_event', struct.pack('>I', 0x99000))

    def test_setup_native_popups_and_transport_specific_visibility(self):
        m = BrowserMachine(); m.app('config_start'); m.active_form = 1000
        m.app('draw')
        self.assertTrue(all(not m.visible[i] for i in (1003, 1004, 1011, 1012)))
        m.action(1002); m.app('draw')
        self.assertTrue(all(m.visible[i] for i in (1003, 1004, 1011, 1012)))
        self.assertEqual(m.control_labels, {1003: b'115200', 1004: b'None'})
        for control, listing, selected in ((1003, 1013, 1), (1004, 1014, 1)):
            m.uc.mem_write(0x99000, struct.pack('>HBBhhHIHIhh', 14, 0, 0, 0, 0, control, 0, listing, 0, selected, 0))
            self.assertTrue(m.app('handle', struct.pack('>I', 0x99000)))
        self.assertEqual(m.control_labels, {1003: b'19200', 1004: b'RTS/CTS'})
        self.assertEqual(m.draft()[8:12], struct.pack('>I', 19200))
        self.assertEqual(m.draft()[6:8], b'\0\x01')
        m.uc.mem_write(0x99000, struct.pack('>HBBhhH', 9, 0, 0, 0, 0, 1003) + bytes(20))
        self.assertFalse(m.app('handle', struct.pack('>I', 0x99000)))
        for transport in (1007, 1001):
            m.action(transport); m.app('draw')
            self.assertTrue(all(not m.visible[i] for i in (1003, 1004, 1011, 1012)))
            before = m.draft(); m.pick(1003, 0); m.pick(1004, 0)
            self.assertEqual(m.draft(), before)
        self.assertFalse(m.requests); m.clean()

    def test_setup_test_result_and_error_text_stay_above_bottom_buttons(self):
        m = BrowserMachine(); m.app('config_start'); m.active_form = 1000
        m.app('config_test'); m.app('draw')
        self.assertTrue(any(b'PASS' in text for _, text in m.drawn_text))
        m.uc.mem_write(m.variable('nioResult'), struct.pack('>H', 5))
        m.drawn_positions.clear(); m.app('draw')
        self.assertTrue(any(b'NIO 5' in text for text, _, _ in m.drawn_positions))
        self.assertTrue(all(108 <= y < 145 for _, _, y in m.drawn_positions))
        m.clean()

    def test_wifi_entry_loads_ssid_and_status_but_scan_is_manual(self):
        m = BrowserMachine(); m.app('config_start'); m.active_form = 1500
        self.event(m, 24)
        self.assertEqual(m.fields[1501], b'Current AP')
        self.assertEqual(m.fields[1502], b'')
        self.assertEqual([r[:2] for r in m.requests], [b'\xf3\x02', b'\xf3\x01'])
        self.assertTrue(any(b'Connected' in text for _, text in m.drawn_text))
        self.assertFalse(m.byte('clientOpen'))
        self.event(m, 9, 1504)
        self.assertEqual(m.requests[-1][:2], b'\xf3\x04')
        self.assertEqual(m.word('scanCount'), 2)
        m.uc.mem_write(0x99000, struct.pack('>HBBhhHIh', 12, 0, 0, 0, 0, 1503, 0, 1) + bytes(16))
        m.app('palm_page_event', struct.pack('>I', 0x99000))
        self.assertEqual(m.fields[1501], b'Other AP')
        self.event(m, 9, 1506); self.assertEqual(m.active_form, 1000)
        m.clean()

    def test_wifi_load_failure_and_unsupported_status_leave_ui_usable(self):
        for error in (5, 8):
            m = BrowserMachine(); m.app('config_start'); m.active_form = 1500
            if error == 5: m.wifi_config_error = error
            else: m.wifi_status_error = error
            self.event(m, 24)
            self.assertEqual(m.fields[1502], b'')
            self.assertFalse(m.byte('clientOpen'))
            self.assertEqual(m.fields[1501], b'' if error == 5 else b'Current AP')
            self.assertEqual(bool(m.alerts), error == 5)
            self.assertFalse(any(r[:2] == b'\xf3\x04' for r in m.requests))
            if error == 8:
                m.fail_device = 0xf3; self.event(m, 9, 1504)
                self.assertTrue(any(b'Scan unavailable' in text for _, text in m.drawn_text))
                self.assertFalse(m.byte('clientOpen'))
            self.event(m, 9, 1506); self.assertEqual(m.active_form, 1000)
            m.clean()

    def test_double_tap_opens_host_directory_and_file_via_existing_actions(self):
        m = BrowserMachine(); m.app('config_start'); m.active_form = 1100
        m.host_record = b'host\n'; self.assertTrue(m.app('palm_hosts_load'))
        self.event(m, 24)
        self.tap(m, 1101, 0, 1); self.assertEqual(m.active_form, 1100)
        self.tap(m, 1101, 0, 2); self.assertEqual(m.active_form, 1200)
        m.directory = [(1, b'games', 0), (0, b'Holdem.prc', 24370)]
        self.event(m, 24)
        before = len(m.requests)
        self.tap(m, 1201, 0, 1); self.assertEqual(len(m.requests), before)
        self.tap(m, 1201, 0, 2)
        self.assertEqual(m.string(m.variable('palmPath')), b'games/')
        self.assertEqual(m.page_requests[-1][0], b'tnfs://host/games/')
        m.alert_results[9101] = 1  # Cancel the normal installation confirmation.
        self.tap(m, 1201, 1, 1); self.assertNotIn(9101, m.alert_ids)
        self.tap(m, 1201, 1, 2); self.assertEqual(m.alert_ids.count(9101), 1)
        self.assertEqual(m.string(m.variable('currentUri')), b'tnfs://host/games/Holdem.prc')
        self.assertFalse(m.byte('clientOpen')); m.clean()

    def test_double_tap_requires_same_item_and_does_not_survive_list_refresh(self):
        m = BrowserMachine(); m.app('config_start'); m.active_form = 1100
        m.host_record = b'one\ntwo\n'; self.assertTrue(m.app('palm_hosts_load'))
        self.event(m, 24)
        self.tap(m, 1101, 0, 1); self.tap(m, 1101, 1, 2)
        self.assertEqual(m.active_form, 1100)
        m.app('show_hosts')
        self.tap(m, 1101, 1, 2); self.assertEqual(m.active_form, 1100)
        m.now += 100
        self.tap(m, 1101, 1, 1); self.assertEqual(m.active_form, 1100)
        self.tap(m, 1101, 1, 2); self.assertEqual(m.active_form, 1200)
        m.clean()

    def test_updated_native_form_layouts(self):
        from test_config import ROOT
        source = (ROOT / 'src/platform/palmos/config.rcp').read_text()
        setup = source.split('FORM ID 1000 ', 1)[1].split('FORM ID 1100 ', 1)[0]
        self.assertLess(setup.index('"Springboard"'), setup.index('"USB"'))
        self.assertLess(setup.index('"USB"'), setup.index('"Serial"'))
        self.assertNotIn('"Status"', setup)
        self.assertIn('POPUPLIST ID 1003 1013', setup)
        self.assertIn('POPUPLIST ID 1004 1014', setup)
        for label in ('WiFi', 'Save', 'Test', 'Reset'):
            self.assertRegex(setup, r'BUTTON "' + label + r'" ID \d+ AT \(\d+ 145 ')
        wifi = source.split('FORM ID 1500 ', 1)[1].split('ALERT ID', 1)[0]
        for label in ('Scan', 'Join', 'Done'):
            self.assertRegex(wifi, r'BUTTON "' + label + r'" ID \d+ AT \(\d+ 145 ')
        self.assertIn('ID 1503 AT (2 95 156 44)', wifi)

    def test_offline_hosts_open_does_not_start_transport_or_block_setup(self):
        m = BrowserMachine(); m.app('config_start'); m.silent = True
        m.active_form = 1100
        self.event(m, 24)
        self.assertFalse(m.requests); self.assertFalse(m.alerts)
        self.assertFalse(m.byte('clientOpen')); self.assertEqual(m.mmio, 0)
        m.uc.mem_write(0x99000, struct.pack('>HBBhhH', 9, 0, 0, 0, 0, 9003) + bytes(16))
        self.assertTrue(m.app('palm_page_tab', struct.pack('>I', 0x99000)))
        self.assertEqual(m.active_form, 1000); self.assertFalse(m.requests); m.clean()

    def test_hosts_scrollbar_and_page_buttons_remain_within_list(self):
        m = BrowserMachine(); m.app('config_start'); m.active_form = 1100
        m.host_record = b''.join(f'host{i}\n'.encode() for i in range(8))
        self.assertTrue(m.app('palm_hosts_load'))
        self.event(m, 24)
        self.assertEqual(m.scrollbars[1108], (0, 0, 1, 7))
        before = len(m.requests)
        self.event(m, 9, 1107)
        self.assertEqual(m.list_tops[1101], 1)
        self.event(m, 33, 1108, 0)
        self.assertEqual(m.list_tops[1101], 0)
        self.event(m, 9, 1106)
        self.assertEqual(m.list_tops[1101], 0)
        self.assertEqual(len(m.requests), before); m.clean()

    def test_browser_thumb_release_fetches_once_and_maps_last_page(self):
        m = BrowserMachine(); m.app('config_start'); m.active_form = 1200
        self.assertTrue(m.parse_hosts(b'host\n'))
        self.event(m, 24)
        self.assertEqual(m.scrollbars[1209], (0, 0, 15, 1))
        before = len(m.requests)
        self.event(m, 34, 1209, 7)  # Tracking does not initiate remote calls.
        self.assertEqual(len(m.requests), before)
        self.event(m, 33, 1209, 15)
        self.assertEqual(m.word('palmBrowseStart'), 120)
        self.assertEqual(m.word('palmEntryCount'), 5)
        self.assertEqual(m.scrollbars[1209], (15, 0, 15, 1))
        before = len(m.requests)
        self.event(m, 33, 1209, 15)
        self.assertEqual(len(m.requests), before); m.clean()

    def test_scrollbar_range_fits_signed_palm_values_at_directory_limit(self):
        m = BrowserMachine(); m.active_form = 1200
        m.uc.mem_write(m.variable('palmBrowseTotal'), struct.pack('>I', 65535))
        m.uc.mem_write(m.variable('palmBrowseStart'), struct.pack('>H', 65528))
        m.app('show_browse')
        self.assertEqual(m.scrollbars[1209], (8191, 0, 8191, 1))

    def test_install_large_callback_and_full_final_chunk_do_not_probe_eof(self):
        m = BrowserMachine(); m.app('config_start'); m.eof_read_error = True
        uri, data = b'tnfs://host/App.prc', bytes(range(256)) * 8
        m.files[uri] = data; m.uc.mem_write(m.variable('currentUri'), uri + b'\0')
        self.assertTrue(m.app('begin'))
        m.uc.mem_write(0x94000, install_state(len(data)))
        m.uc.mem_write(0x96000, struct.pack('>I', 4096))
        error = m.app('install_read', struct.pack('>III', 0x98000, 0x96000, 0x94000)) & 0xffff
        self.assertEqual(error, 0); self.assertEqual(m.word('nioResult'), 0)
        self.assertEqual(m.read32(0x96000), len(data))
        self.assertEqual(bytes(m.uc.mem_read(0x98000, len(data))), data)
        before = len(m.requests)
        m.uc.mem_write(0x96000, struct.pack('>I', 1024))
        self.assertEqual(m.app('install_read', struct.pack('>III', 0x98000, 0x96000, 0x94000)) & 0xffff, 0)
        self.assertEqual(m.read32(0x96000), 0); self.assertEqual(len(m.requests), before)
        m.app('config_end_services'); m.clean()

    def test_install_stops_with_an_error_when_the_device_makes_no_progress(self):
        # A reply with no bytes and no EOF would repeat for ever without the guard.
        m = BrowserMachine(); m.app('config_start'); m.empty_without_eof = True
        uri, data = b'tnfs://host/App.prc', bytes(range(256)) * 2
        m.files[uri] = data; m.uc.mem_write(m.variable('currentUri'), uri + b'\0')
        self.assertTrue(m.app('begin'))
        m.uc.mem_write(0x94000, install_state(len(data)))
        m.uc.mem_write(0x96000, struct.pack('>I', 64))
        self.assertNotEqual(m.app('install_read', struct.pack('>III', 0x95000, 0x96000, 0x94000)) & 0xffff, 0)
        self.assertEqual(m.read32(0x96000), 0)
        self.assertEqual(len([r for r in m.requests if r[:2] == b'\xfe\x03']), 1)  # no replay
        m.app('config_end_services'); m.clean()

    def test_install_fails_when_the_file_ends_before_its_stat_size(self):
        # The device says EOF at 100 bytes while stat promised 150: a truncated file must
        # fail the install, not hand the OS a short stream as if it were complete.
        m = BrowserMachine(); m.app('config_start')
        uri, data = b'tnfs://host/App.prc', bytes(range(100))
        m.files[uri] = data; m.uc.mem_write(m.variable('currentUri'), uri + b'\0')
        self.assertTrue(m.app('begin'))
        m.uc.mem_write(0x94000, install_state(150))
        m.uc.mem_write(0x96000, struct.pack('>I', 4096))
        self.assertNotEqual(m.app('install_read', struct.pack('>III', 0x98000, 0x96000, 0x94000)) & 0xffff, 0)
        m.app('config_end_services'); m.clean()

    def test_install_retries_io_only_when_transport_needs_no_recovery(self):
        for recovery, failures, code, expected_calls in (
                (False, 1, 5, 2), (False, 9, 5, 3), (True, 1, 5, 1), (False, 1, 6, 1)):
            m = BrowserMachine(); m.app('config_start')
            uri, data = b'tnfs://host/App.prc', b'x' * 100
            m.files[uri] = data; m.uc.mem_write(m.variable('currentUri'), uri + b'\0')
            self.assertTrue(m.app('begin'))
            m.read_failures = failures; m.force_read_recovery = recovery
            m.read_failure_code = code
            m.uc.mem_write(0x94000, install_state(len(data)))
            m.uc.mem_write(0x96000, struct.pack('>I', 100))
            error = m.app('install_read', struct.pack('>III', 0x98000, 0x96000, 0x94000)) & 0xffff
            self.assertEqual(bool(error), recovery or failures > 3 or code != 5)
            self.assertEqual(len([r for r in m.requests if r[:2] == b'\xfe\x03']), expected_calls)
            original = m.word('nioResult')
            m.app('config_end_services')
            self.assertEqual(m.word('nioResult'), original)
            m.clean()

    def test_install_chunks_fit_negotiated_packet_and_scratch_limits(self):
        for transport in (1, 2, 3):
            for packet, chunk in ((1024, 256), (529, 256), (528, 256), (250, 233)):
                with self.subTest(transport=transport, packet=packet):
                    m = BrowserMachine(); m.app('config_start')
                    if transport != 1:
                        m.action(1002 if transport == 2 else 1007); m.app('config_save')
                    m.packet_limit = packet
                    uri, data = b'tnfs://host/App.prc', bytes(range(256)) * 4 + b'end'
                    m.files[uri] = data; m.eof_read_error = True
                    m.uc.mem_write(m.variable('currentUri'), uri + b'\0')
                    self.assertTrue(m.app('begin'))
                    self.assertEqual(m.word('serviceCapacity'), min(packet - 7, 512))
                    m.uc.mem_write(0x94000, install_state(len(data)))
                    m.uc.mem_write(0x96000, struct.pack('>I', len(data) + 512))
                    self.assertEqual(m.app('install_read', struct.pack('>III', 0x98000, 0x96000, 0x94000)), 0)
                    self.assertEqual(m.read32(0x96000), len(data))
                    self.assertEqual(bytes(m.uc.mem_read(0x98000, len(data))), data)
                    reads = [r for r in m.requests if r[:2] == b'\xfe\x03']
                    expected = [min(chunk, len(data) - at) for at in range(0, len(data), chunk)]
                    self.assertEqual([int.from_bytes(r[-2:], 'little') for r in reads], expected)
                    self.assertTrue(all(7 + 10 + size <= packet for size in expected))
                    self.assertTrue(m.app('config_end_services')); m.clean()

    def test_install_popup_starts_empty_and_meter_handles_large_sizes(self):
        m = BrowserMachine()
        m.uc.mem_write(0x90000, b'FI-Holdem.prc\0')
        m.app('install_popup', struct.pack('>II', 0x90000, 24370))
        self.assertEqual(m.active_form, 1600)
        self.assertIn((b'FI-Holdem.prc', 8, 22), m.drawn_positions)
        self.assertIn((b'0 of 24370 bytes', 8, 50), m.drawn_positions)
        self.assertFalse(m.meter_fills)
        self.assertEqual(m.meter_frames[-1], (8, 80, 128, 10))
        for total in (1, 127, 24370, 0xffffffff):
            widths = []
            for done in (0, total // 2, total):
                m.meter_fills.clear()
                m.app('install_progress', struct.pack('>II', done, total))
                widths.append(m.meter_fills[-1][2] if m.meter_fills else 0)
            self.assertEqual(widths[0], 0)
            self.assertEqual(widths[-1], 126)
            if total > 1: self.assertTrue(62 <= widths[1] <= 63)
            self.assertEqual(widths, sorted(widths))
            self.assertTrue(all(0 <= width <= 126 for width in widths))

    def test_install_popup_restores_browser_on_os_error_and_allocation_failure(self):
        for fail_alloc in (False, True):
            with self.subTest(fail_alloc=fail_alloc):
                m = BrowserMachine(); m.app('config_start')
                m.active_form = 1200; m.fail_popup_alloc = fail_alloc
                uri = b'tnfs://host/App.prc'
                m.files[uri] = bytes(100)
                m.uc.mem_write(m.variable('currentUri'), uri + b'\0')
                m.uc.mem_write(0x90000, b'App.prc\0' + bytes(220))
                m.app('install_file', struct.pack('>I', 0x90000))
                self.assertEqual(m.active_form, 1200)
                self.assertFalse(m.byte('clientOpen'))
                if fail_alloc:
                    self.assertNotIn('delete', m.window_events)
                    self.assertIn(b'Not enough memory', m.alerts[-1])
                else:
                    self.assertEqual(m.assert_popup_active, 1600)
                    self.assertEqual(m.window_events[-3:], ['erase', 'delete', ('active', 1200)])
                    self.assertIn(b'Installation failed', m.alerts[-1])
                m.clean()

    def test_install_progress_is_throttled_and_final_count_is_always_drawn(self):
        for ticks, start in ((0, 0), (40, 0), (40, 0xffffff80), (150, 0)):
            with self.subTest(ticks=ticks, start=start):
                m = BrowserMachine(); m.app('config_start')
                uri, data = b'tnfs://host/App.prc', bytes(range(256)) * 12
                m.files[uri] = data; m.read_ticks = ticks; m.now = start
                m.uc.mem_write(m.variable('currentUri'), uri + b'\0')
                self.assertTrue(m.app('begin'))
                m.drawn_text.clear(); m.auto_off_resets = 0
                m.uc.mem_write(0x94000, install_state(len(data)))
                m.uc.mem_write(0x96000, struct.pack('>I', len(data)))
                self.assertEqual(m.app('install_read', struct.pack('>III', 0x98000, 0x96000, 0x94000)), 0)
                self.assertEqual(bytes(m.uc.mem_read(0x98000, len(data))), data)
                progress = [(time, text) for time, text in m.drawn_text if b' of ' in text]
                self.assertEqual(progress[0][1], b'256 of 3072 bytes')
                self.assertEqual(progress[-1][1], b'3072 of 3072 bytes')
                widths = [rect[2] for rect in m.meter_fills]
                self.assertEqual(widths, sorted(widths))
                self.assertEqual(widths[-1], 126)
                self.assertTrue(all(((b[0] - a[0]) & 0xffffffff) >= 100
                                    for a, b in zip(progress, progress[1:-1])))
                self.assertEqual(m.auto_off_resets, len(progress))
                if not ticks: self.assertEqual(len(progress), 2)
                elif ticks < 100:
                    self.assertLess(len(progress), 12)
                    self.assertLess((progress[-1][0] - progress[-2][0]) & 0xffffffff, 100)
                self.assertTrue(m.app('config_end_services')); m.clean()

    def test_install_uses_compatible_reads_even_when_library_advertises_larger_packets(self):
        m = BrowserMachine(); m.app('config_start')
        # Model the reported size-dependent failure, not a TNFS protocol limit.
        m.read_limit = 256
        uri, data = b'tnfs://host/App.prc', bytes(range(256)) * 3
        m.files[uri] = data; m.eof_read_error = True
        m.uc.mem_write(m.variable('currentUri'), uri + b'\0')
        self.assertTrue(m.app('begin'))
        m.uc.mem_write(0x94000, install_state(len(data)))
        m.uc.mem_write(0x96000, struct.pack('>I', len(data)))
        self.assertEqual(m.app('install_read', struct.pack('>III', 0x98000, 0x96000, 0x94000)), 0)
        self.assertEqual(m.read32(0x96000), len(data))
        self.assertEqual(bytes(m.uc.mem_read(0x98000, len(data))), data)
        reads = [r for r in m.requests if r[:2] == b'\xfe\x03']
        self.assertEqual([int.from_bytes(r[-2:], 'little') for r in reads], [256] * 3)
        self.assertTrue(m.app('config_end_services')); m.clean()

    def test_info_reports_failed_service_and_stops_the_batch(self):
        m = BrowserMachine(); m.app('config_start'); m.active_form = 1300
        m.fail_device = 0x70
        self.event(m, 24)
        self.assertEqual([r[0] for r in m.requests], [0x70])
        self.assertTrue(any(b'Firmware read failed' in a and b'NIO 5' in a for a in m.alerts))
        self.assertFalse(m.byte('clientOpen')); m.clean()


if __name__ == '__main__': unittest.main()
