# TRS-80 Model 4/4P config-nio (`CONFIG/CMD`)

Full-screen configuration for a Model 4/4P running LS-DOS 6.3.x, equivalent to the MS-DOS config-nio:
**Hosts**, **Browse** (files left, `:0`-`:7` drives right), **Slots** and **Setup** (Wi-Fi and
preferences). 80x24 ASCII text. Written in C for z88dk (sccz80); the state, store and FujiNet service
layers are the shared ones in `src/platform/portable/`, so the saved hosts, mappings, slot catalogue and
preferences are the same records every other platform reads.

**Status: verified in MAME (`trs80m4p`) against a fake fujinet-nio; not yet verified on a real 4P.**

## Keys

The 4P has no ESC or TAB key, and its arrows are control codes (measured in MAME), so:

| Key | Action |
|---|---|
| Up / Down | move |
| SHIFT+Up / SHIFT+Down | page up / down |
| SHIFT+Left / SHIFT+Right | previous / next tab |
| Right arrow (code 09h) | TAB: switch Files / Drives pane |
| Left arrow (code 08h, Backspace) | up a folder in Browse; deletes a character in text fields |
| ENTER | open folder, mount the image on the selected drive, accept |
| BREAK | cancel a prompt; quit |
| H B S W | jump to Hosts / Browse / Slots / Setup |
| Q | quit |
| X / R | (drives) eject / toggle read-only; (Browse files) R reloads the folder |
| A E D | (Hosts) add / edit / delete a host |
| N J D Z | (Setup) scan Wi-Fi, join by name, date order, file size style |

There is no reverse video: the selected row is marked `>`. Text fields edit at the end only.

## Files

- `trs80/` Makefile, `crt0.asm`, `config.c`; sources in `src/platform/trs80/` (`config_ui.c` screens,
  `trs80_console.*` over the LS-DOS `@VDCTL`/`@KEY` SVCs, `ui_widgets.*`, `fnctl_trs80.c`/`nio_glue.asm`
  transport glue, `wifi_trs80.c`).
- Build limits (`trs80/Makefile`): 4 hosts, 12 browse entries per page, 127-byte URIs.
- `make trs80` builds `trs80/build/config.bin`; `make test-trs80-host` runs the host widget tests.

## Mounting and FUJIDRV

CONFIG asks the FujiNet to mount the image and records it in the slot catalogue and drive map. It does not
install the LS-DOS serial driver: only drive `:0` has it from boot (`nioboot`). For `:1`-`:7` run `FUJIDRV :n`
once from LS-DOS; CONFIG shows that as a hint after mounting. (Detecting an installed driver needs LS-DOS
drive-table access from C and is not implemented.)

### Drive `:0` is protected

`:0` holds the boot disk (LS-DOS, the F* commands and CONFIG itself). CONFIG shows it as `(boot disk)`, starts with
`:1` selected, and refuses to mount, eject, edit or flip read-only on `:0`. Use `FMOUNT :0 <uri>` to replace
the boot disk deliberately.

## Screen updates

Moving a selection (Up/Down) repaints only the old and new rows (plus the `n/total` counter), not the screen.
A scroll, a page change or any other key repaints the whole screen. Each row is written as one string.

## Memory map

The program loads at 3000h and must end below D000h, where the boot repo's `nioclient_config.asm` (the
SLIP/UART transport, entry `nio_request` at D003h) is loaded in the same `/CMD`. `trimbss.sh` cuts the
uninitialised data from the file and fails the build if the image would pass D000h. LS-DOS enters at
SP near 02DCh; `crt0.asm` moves the stack to D000h. Interrupts are off while a request is in flight.

## z88dk pitfalls (found the hard way)

- `-O0` fixes nothing and makes the image 44 KB. Do not use it.
- 32-bit `+` and unsigned `>>`/`/ 4` on `uint32_t` (`l_long_add`, `l_long_asr_u`) unbalance the stack
  on the Z80 with this library: arguments after them read the wrong slot. `fnsvc_list_pages` and
  `config_ui.c` do page arithmetic in 16 bits for that reason. 32-bit compare, `%`, `/ 10` work.
- Library code placed after `bss` in `crt0.asm`'s section list is cut from the `/CMD`; keep that order.
- `fujinet-nio-lib`'s `fn_wifi.c` does not compile under sccz80, so Wi-Fi uses `wifi_trs80.c`.

## Tests

The cheapest test that fails for each layer. The boot repo (`~/dev/boot-trs80-4p`) holds the end-to-end
ones: `MAME_TESTS=1 python3 -m unittest tests.test_mame_config` boots LS-DOS in MAME, runs the `/CMD` with a
fake fujinet-nio on the serial pty and asserts on screen text and recorded requests (hello, widgets,
fnsvc, store, browse/mount/eject, hosts, Wi-Fi/prefs). `tools/demo_config.py` there opens a MAME window with
CONFIG running against the fake.

## Boot disk

`tools/make_boot_image.py` (boot repo) adds `CONFIG/CMD` (program + client). It needs about 22 granules, so it
drops LS-DOS BASIC (`BASIC/CMD`, `BASIC/OV1`, `BASIC/OV2`, `BREF/CMD`) on top of `HELP/CMD` and `DOS/HLP`;
`--no-config` keeps BASIC, `--drop NAME/EXT` removes more. It fails loudly if CONFIG does not fit.
