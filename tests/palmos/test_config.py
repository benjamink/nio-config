"""Execute the real Palm app and ABI 1.2 library; mock OS and remote peer only.
No Palm, serial port, USB, firmware or running NIO service is accessed.
"""
import json
import os
import re
import struct
import sys
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
LIB = Path(os.environ.get('FUJINET_NIO_LIB', ROOT.parent / 'fujinet-nio-lib')).resolve()
sys.path.insert(0, str(LIB / 'tests'))
import test_palmos_shared as foundation
from test_palmos_shared_transports import TransportMachine, config
from test_palmos_shared_usb import USBMachine
from unicorn.m68k_const import UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_PC
OUT = Path(os.environ.get('PALMOS_CONFIG_BUILD', ROOT / 'build/palmos-usb'))

class ConfigMachine(USBMachine):
    def __init__(self):
        self.missing = self.incompatible = self.no_caps = False
        self.bad_readback = False
        self.old_usb = False
        self.removes = 0
        super().__init__()
        sections = foundation.image.coff_sections((OUT / 'NioConfig').read_bytes())
        by_name = {s['name']: s for s in sections}
        d, b = by_name['.data'], by_name['.bss']
        data = bytearray(d['data']) + bytearray(b['size'])
        for off in range(0, len(by_name.get('.reloc', {}).get('data', b'')), 12):
            kind, source, pos, target, reserved = struct.unpack_from('>HHIHH', by_name['.reloc']['data'], off)
            assert kind == 1 and reserved == 0 and sections[source]['name'] == '.data'
            target_name = sections[target]['name']
            assert target_name in ('.text', '.data', '.bss')
            value = struct.unpack_from('>I', data, pos)[0]
            struct.pack_into('>I', data, pos, value + (self.APP if target_name == '.text' else self.app_data))
        self.uc.mem_write(self.APP, by_name['.text']['data'])
        self.uc.ctl_remove_cache(self.APP, self.APP + 0x8000)
        self.uc.mem_write(self.app_data, bytes(data))
        self.app_symbols = {n:int(a,16) for a,_,n in re.findall(
            r'^([0-9a-f]+)\s+(\S)\s+(\S+)$', (OUT/'NioConfig.symbols').read_text(), re.M)}
        self.app_a5 = self.app_data + self.app_symbols['edata']

    def hook(self, uc, pc, size, context):
        if bytes(uc.mem_read(pc,2)) == b'\x4e\x4f':
            trap, sp = self.read16(pc+2), uc.reg_read(UC_M68K_REG_A7)
            if trap == 0xa806 and self.old_usb:
                uc.mem_write(self.read32(sp+2), struct.pack('>I',0x8000001f))
                uc.reg_write(UC_M68K_REG_D0,0); uc.reg_write(UC_M68K_REG_PC,pc+4)
                return
            if trap == 0xa0b8: self.removes += 1
            if trap == 0xa2d3 and self.bad_readback and self.saved_preferences:
                # Simulate a valid-but-different persisted readback through
                # the real GetConfig trap, not a replacement Save function.
                key=(0x4e694c31,0x4e34)
                if key in self.saved_preferences:
                    self.saved_preferences[key]=(1,config(2,9600))
            if ((trap == 0xa2ac and self.missing) or
                    (trap == 0xa805 and self.incompatible) or
                    (trap == 0xa806 and self.no_caps)):
                result = 1 if self.missing else 0
                if trap == 0xa805: uc.mem_write(self.read32(sp+2), struct.pack('>4H',8,1,1,3))
                if trap == 0xa806: uc.mem_write(self.read32(sp+2), bytes(4))
                uc.reg_write(UC_M68K_REG_D0,result); uc.reg_write(UC_M68K_REG_PC,pc+4)
                return
        super().hook(uc, pc, size, context)

    def app(self, name, args=b''):
        return self.call(name, args, app=True, a5=self.app_a5)
    def variable(self, name): return self.app_data + self.app_symbols[name]
    def word(self, name): return self.read16(self.variable(name))
    def byte(self, name): return self.uc.mem_read(self.variable(name),1)[0]
    def draft(self): return bytes(self.uc.mem_read(self.variable('config'),24))
    def action(self, ident): self.app('config_action',struct.pack('>H',ident))
    def text(self):
        address = self.read32(self.variable('message'))
        return bytes(self.uc.mem_read(address,80)).split(b'\0')[0].decode()
    def clean(self):
        self.app('config_stop')
        assert not self.globals and not self.sessions and not self.serial_opened

class PalmConfigTests(unittest.TestCase):
    def test_metadata_resources_and_no_embedded_core(self):
        p=(OUT/'NioConfig.prc').read_bytes()
        self.assertEqual(p[:32].split(b'\0')[0],b'NIO Config')
        self.assertEqual(p[60:68],b'applNiC1')
        metadata,resources=foundation.image.prc(p)
        self.assertIn((b'tFRM',1000),resources)
        self.assertIn((b'tAIN',1000),resources)
        self.assertIn((b'code',1),resources)
        self.assertEqual(metadata['creator'],'NiC1')
        symbols=(OUT/'NioConfig.symbols').read_text()
        for forbidden in ('fn_packet_', 'fn_transport_', 'fn_palmos_', 'fn_sessions', 'nl_cs1_', 'nl_serial_', 'PrefSetAppPreferences', 'PrefGetAppPreferences'):
            self.assertNotIn(forbidden,symbols)
        dis=(OUT/'NioConfig.dis').read_text().lower()
        for trap in ('a813','a814','a815','a811'):
            self.assertIn(trap,dis)
        self.assertIn('USB cradle: unavailable',(ROOT/'src/platform/palmos/config.c').read_text())

    def test_usb_save_relaunch_clock_and_no_serial_controls(self):
        m=ConfigMachine(); m.app('config_start'); m.action(1007)
        self.assertEqual(m.draft(),config(3))
        m.action(1003); m.action(1004); self.assertEqual(m.draft(),config(3))
        m.app('config_save'); self.assertEqual(m.word('palmError'),0)
        self.assertFalse(m.usb_calls); m.clean()
        m.app('config_start'); self.assertEqual(m.draft(),config(3))
        m.app('config_test'); self.assertIn('PASS',m.text())
        self.assertFalse(m.usb_live); self.assertFalse(m.byte('clientOpen')); m.clean()

    def test_default_no_transport_open_or_implicit_save(self):
        m=ConfigMachine(); m.app('config_start')
        self.assertEqual(m.draft(),config())
        self.assertEqual(m.word('configFlags'),1)
        self.assertFalse(m.saved_preferences); self.assertEqual(m.mmio,0); self.assertFalse(m.serial_calls)
        self.assertEqual(m.word('palmError'),0); m.clean(); self.assertFalse(m.registered)

    def test_missing_library_and_incompatible_abi_or_caps(self):
        for mode in ('missing','incompatible','no_caps'):
            with self.subTest(mode=mode):
                m=ConfigMachine(); setattr(m,mode,True); m.app('config_start')
                self.assertNotEqual(m.word('palmError'),0)
                self.assertFalse(m.byte('libraryOpen')); self.assertFalse(m.globals)
                self.assertFalse(m.registered); self.assertEqual(m.mmio,0)

    def test_serial_selection_baud_flow_save_readback_and_relaunch(self):
        m=ConfigMachine(); m.app('config_start'); m.action(1002); m.action(1003); m.action(1004)
        self.assertEqual(m.draft(),config(2,9600,1)); m.app('config_save')
        self.assertEqual(m.word('palmError'),0); self.assertFalse(m.byte('dirty'))
        self.assertEqual(m.saved_preferences[(0x4e694c31,0x4e34)],(1,config(2,9600,1)))
        self.assertEqual(m.mmio,0); self.assertFalse(m.serial_calls)
        m.clean(); m.app('config_start'); self.assertEqual(m.draft(),config(2,9600,1)); m.clean()

    def test_all_baud_values_and_usb_not_selectable(self):
        m=ConfigMachine(); m.old_usb=True; m.app('config_start'); m.action(1002)
        for baud in (9600,19200,38400,57600,115200):
            m.action(1003); self.assertEqual(struct.unpack_from('>I',m.draft(),8)[0],baud)
        before=m.draft(); m.action(1007); self.assertEqual(m.draft(),before)
        m.uc.mem_write(m.variable('config')+4,b'\0\3'); m.app('config_save')
        self.assertNotEqual(m.word('palmError'),0); self.assertFalse(m.saved_preferences); m.clean()

    def test_malformed_preferences_explicit_repair_only(self):
        for data in (config(version=99),config(baud=1),config(reserved=1),b'bad'):
            m=ConfigMachine(); m.saved_preferences[(0x4e694c31,0x4e34)]=(1,data)
            m.app('config_start'); self.assertEqual(m.word('configFlags'),2)
            self.assertEqual(m.saved_preferences[(0x4e694c31,0x4e34)][1],data)
            m.app('config_test'); self.assertNotEqual(m.word('nioResult'),0)
            self.assertEqual(m.mmio,0); m.app('config_save'); self.assertEqual(m.word('palmError'),0); m.clean()

    def test_save_failure_and_invalid_draft(self):
        m=ConfigMachine(); m.app('config_start'); m.action(1002); m.pref_write_failure=True
        m.app('config_save'); self.assertNotEqual(m.word('palmError'),0); self.assertTrue(m.byte('dirty'))
        m.uc.mem_write(m.variable('config')+8,struct.pack('>I',123))
        m.app('config_save'); self.assertNotEqual(m.word('palmError'),0); m.clean()

    def test_clock_success_and_repeated_session_cleanup(self):
        m=ConfigMachine(); m.app('config_start')
        for _ in range(3):
            m.app('config_test'); self.assertEqual(m.word('palmError'),0)
            self.assertEqual(m.word('nioResult'),0); self.assertIn('PASS',m.text())
            self.assertFalse(m.byte('clientOpen')); self.assertFalse(m.sessions)
        self.assertTrue(m.requests); m.clean()

    def test_absent_selected_transport_failure_is_usable(self):
        for serial in (False,True):
            m=ConfigMachine(); m.app('config_start')
            if serial: m.action(1002); m.app('config_save'); m.serial_available=False
            else: m.present=False
            m.app('config_test'); self.assertTrue(m.word('nioResult') or m.word('palmError'))
            self.assertFalse(m.byte('clientOpen')); self.assertFalse(m.serial_opened)
            m.action(1001); m.app('config_save'); self.assertEqual(m.word('palmError'),0); m.clean()

    def test_unsaved_draft_never_tests_other_transport(self):
        m=ConfigMachine(); m.app('config_start'); m.action(1002); m.app('config_test')
        self.assertIn('Save',m.text()); self.assertFalse(m.requests); self.assertFalse(m.serial_calls); m.clean()

    def test_clock_failure_ends_client_and_does_not_retry(self):
        m=ConfigMachine(); m.app('config_start'); m.forced_result=1; m.app('config_test')
        self.assertNotEqual(m.word('nioResult'),0); self.assertFalse(m.byte('clientOpen'))
        self.assertEqual(m.doorbells,1); m.clean()

    def test_save_readback_mismatch_is_reported(self):
        m=ConfigMachine(); m.app('config_start'); m.bad_readback=True; m.app('config_save')
        self.assertNotEqual(m.word('palmError'),0); self.assertIn('failed',m.text()); m.clean()

    def test_serial_clock_through_installed_dispatcher(self):
        m=ConfigMachine(); m.app('config_start'); m.action(1002); m.app('config_save')
        m.app('config_test'); self.assertEqual(m.word('palmError'),0)
        self.assertEqual(m.word('nioResult'),0); self.assertIn('PASS',m.text())
        self.assertFalse(m.serial_opened); self.assertFalse(m.byte('clientOpen'))
        self.assertEqual(m.mmio,0); m.clean()

    def test_connection_begin_busy_keeps_error_and_other_owner(self):
        m=ConfigMachine(); m.app('config_start')
        token=m.begin()[1]
        m.app('config_test'); self.assertNotEqual(m.word('palmError'),0)
        self.assertFalse(m.byte('clientOpen'))
        # Our app's Close must not remove the still-owned library.
        m.app('config_stop'); self.assertTrue(m.globals)
        m.end(token); m.clean()

    def test_app_stop_balances_reference_after_transient_usb_close_failure(self):
        m=ConfigMachine(); m.app('config_start'); m.action(1007); m.app('config_save')
        m.usb_close_fail=True
        m.app('config_test'); m.app('config_stop')
        address=m.globals
        self.assertTrue(m.usb_live); self.assertTrue(address)
        self.assertFalse(m.byte('libraryOpen')); self.assertFalse(m.byte('clientOpen'))
        self.assertEqual(m.read16(address+m.symbols['nl_opens']),0)
        self.assertEqual(m.removes,0)
        # A different application's Open can clean retained physical resources;
        # no retry state from config_stop is needed and no live storage is freed.
        m.current_app+=1; m.usb_close_fail=False
        self.assertEqual(m.open(),0); self.assertFalse(m.usb_live)
        self.assertEqual(m.close(),(0,0)); self.assertFalse(m.globals)

    def test_existing_library_other_reference_is_never_removed(self):
        m=ConfigMachine(); m.registered=True; self.assertEqual(m.open(),0)
        m.app('config_start'); self.assertFalse(m.byte('libraryLoaded'))
        m.app('config_stop'); self.assertTrue(m.globals); self.assertTrue(m.registered); self.assertEqual(m.removes,0)
        self.assertEqual(m.close(),(0,0))

if __name__=='__main__': unittest.main()
