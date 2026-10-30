# Atari config

`config-nio` for the Atari (`src/platform/atari/`) is the machine's launcher.
It is the program on the default FujiNet boot disk, so a freshly started Atari
shows it. Unlike the Amiga and MS-DOS builds, which mount a disk and return to
the host system, the Atari's job is to **boot**: pick the images, assign them
to D1-D8, press OPTION, and the Atari restarts with those drives mounted.

## Screen

```
 ◢◣ FujiNet NIO Config            192.168.1.20
  Hosts   Browse   Drives   Setup
 tnfs://fujinet.online/Atari/games/             <- path / URI field
 ..                                      <DIR>  <- selection bar (orange)
 alpha.atr                                180K
 beta.atr                                 180K
                                                ^
 1 of 42+                                       v  <- scroll bar
 RET Open BS Up R Reload OPT Boot               <- keys for this tab
 D2 <- alpha.atr                                <- status
```

| Tab | What it does |
|-----|--------------|
| Hosts | Up to 8 filesystems (`sd0:/`, `host:/`, `tnfs://server/path`, `http://...`). RET browses, A adds, E edits, D deletes. |
| Browse | The chosen host's directory, one page of 18 entries at a time. Up/down move the cursor and carry on into the next or previous page at the edges; left/right (or `b`/SPACE) change page, and left on the first page goes up a level (so does BS). RET enters a directory or assigns the image under the cursor: 1-8 picks the drive, R toggles write protect. `R` makes the FujiNet read the folder again. Opened from the Drives tab, the chosen image goes straight to that drive. |
| Drives | D1-D8 with their images. RET picks an image for the selected drive, X ejects, R toggles write protect. |
| Setup | Wi-Fi state, IP/gateway/DNS, MAC and firmware. RET scans for networks (or types an SSID) and attempts to join. The FujiNet saves new credentials only after connecting; if the attempt fails it retains the previous configuration and tries to reconnect it. The screen reports the result so the details can be retried. |

Keys: Ctrl+up/down (or `-` / `=`) move, SPACE and `b` page, TAB (or SELECT)
changes tab, **OPTION or START boots**. The joystick works too: stick moves,
the trigger is RET. H or HELP shows the current tab's commands; any key closes it.
In Browse, F or / edits the name filter. Matching ignores ASCII case; `*` matches
any number of characters and `?` matches one. Without wildcards, text matches
anywhere in the name. A blank filter clears it. Folders remain visible and the
filter persists when entering or leaving a folder; selecting another host clears it.
The FujiNet firmware must support the filter for filtered results to be returned.
The Wi-Fi picker shows up to six scanned access points, with signal in dBm.
Repeated named networks are combined using the strongest signal among those
results. An empty SSID is shown as `(hidden)`; selecting it asks for the name.
The connection wait remains on a flat screen, and the Setup link state refreshes
automatically if restoring the previous network is still in progress. Setup
information is cached across tab changes and browsing; press R to refresh it
after changes made outside the app. Joining Wi-Fi refreshes it automatically.

Wi-Fi joining requires firmware with the trial-join command; older firmware is
reported as needing an update and its saved network is left unchanged.

Each row has its own colours. A display list interrupt at the end of every row
(`atari_dli.s`) loads the next row's background and text colour from two 24
byte tables, so the title, selection bar, hint line and dialogs are whole-row
colour bars on a plain ANTIC mode 2 screen. A vertical-blank handler restarts
the row count every frame so a lost interrupt cannot leave the colours out of
step. The OS attract-mode colour shift and dimming mask apply to every row;
pressing a key restores the normal colours.

## Booting

OPTION (or START) mounts every assigned drive, unmounts D2-D8 that are not
assigned (D1 keeps the config disk until it is assigned), and cold starts the
Atari. The FujiNet keeps its mounts across the reset, so the Atari boots from
whatever is now in D1. DiskDevice slots are numbered from 1: D1 is slot 1.
During mounting, a monochrome screen stays visible with row-colour interrupts
off. If a mount fails, the normal tab and its error message return.

XEX/COM programs are mounted like any image; the firmware presents them as a
boot disk (MyPicoDOS PicoBoot wrapper) so the same drive-assignment flow boots
them.

New drive assignments default to read-only. Use R in the assignment dialog or
on the Drives tab to permit writes. Existing saved assignments keep their mode.

## Data

The app reads and writes the same records as the other front ends
(`docs/state-model.md`): the `hosts` text and the `mappings` v1 record in
the `config-nio` app-store namespace, and the slot catalogue (drive Dn uses
catalogue slot n-1). A drive map made on another machine type is read as is.

## Boot selection and banked memory

Build both variants from the same source with `make TARGET=atari` and
`make atari-banked`. The default Atari boot disk carries `autorun.xex` (the
selector), `standard.xex` and `banked.xex`. PicoBoot loads the selector first.
It tests that two 130XE-compatible banks are independent and restores the bytes
it probes. A 64 KB machine uses the standard image; a machine with those two
banks uses the banked image. The banked XEX needs the selector: it cannot be
loaded directly without first selecting the banks.

The selector uses the resident entry points and DD sector layout of the pinned
PicoBoot binary; the boot-disk build checks the binary hash. The selector reads
its two executables from the first MyDOS directory sector. The default manifest
keeps all three filenames there. On RESET a resident hook reselects the main
bank before restarting the app.

In the banked image, shared code, buffers, display list and interrupts remain
in main RAM outside `$4000-$7FFF`. Hosts, Browse and Drives share the first
16 KB bank; Setup and its Wi-Fi routines occupy the second. Resident gates
switch to Setup and back, and the XEX load-time initializers load each bank
in turn. ANTIC stays in main RAM. The standard layout maps the same named tab
segments into main RAM; it does not switch banks. See the map files under
`build/atari/bin/` and `build/atari-banked/bin/` for exact sizes and remaining
space. The banked layout uses the upper 8 KB of the Setup bank as a packed
page cache. Small resident assembly gates copy records between that bank and
main RAM; ANTIC and interrupt handlers remain in main RAM.

Headless boot tests exercise the actual selector with 64 KB and 128 KB Altirra
memory models, including tab changes and warm RESET. They require both builds:

```sh
source ../../scripts/env.sh
make TARGET=atari
make atari-banked
cd tests/atari
NIO_ATARI_BOOT=1 python3 -m unittest test_boot -v
```

## Memory

The Atari is the tightest target: the portable layer (`src/platform/portable`)
does not fit, so the Atari has a compact core of its own (`atari_core.c`) and
does not use `fnsvc`, `fnctl` or conio.

* `$0650-$0BFF` (page 6 after the display list, and the pages PicoBoot's loader used)
  holds buffers that need no initial value. In the standard build this includes the
  page cache; in the banked build it holds one cache-record staging buffer instead.
* The standard layout in `makefiles/cc65-atari-nio.cfg` loads at `$0C00` up to
  `$BC00`. The banked layout splits resident code around the bank window.
* The first load chunk (`atari_basicoff.s`) switches the BASIC ROM off with
  PORTB and moves `MEMTOP` to `$BC00`, so the program may load into the RAM
  under BASIC and cc65's C stack (set from `MEMTOP`) lands at the top of RAM.
  Without it a disk boot with BASIC in would put the stack inside the program.
  XL/XE only.
* The OS screen is not used. The display list lives at `$0600`; the 960 byte
  screen is in BSS, placed so that no row crosses a 4K boundary (ANTIC wraps a
  line inside its 4K block).
* The OS screen is rebuilt at `$BC40` on RESET, so the program stops at `$BC00`
  (C stack below it). The first chunk also sets `BASICF` (BASIC stays off across
  RESET) and points `DOSVEC` at the program start, so RESET restarts the app.
* URIs may be 196 characters (host + path + name); a saved host 100. The buffers
  that hold them are shared by design; atari_app.c lists who may use each.
* Saved hosts live packed in one 1200 byte pool (up to 24); a drive keeps only
  the last 100 characters of its URI for display and the catalogue holds the
  rest. Two 1000 byte screens make up the double buffer.
* One 512 byte buffer serves the app store, slot catalogue and directory pages;
  host text and Wi-Fi scan results borrow the directory page buffer.
* URI composition is in `atari_compose.s`, with zero-page arguments to avoid
  cc65's parameter-passing overhead. `tests/atari/test_compose.py` compares it
  with the original C rules and checks output limits under `soft65c02`.

`ld65` fails the build if the program no longer fits, which is the memory check.

## Browsing large folders

A directory can have thousands of entries and live on a slow TNFS server, so the
Atari never asks for all of it. The FujiNet lists and sorts the folder once
(directories first, then by name), remembers it, and answers `ListPages`
requests (see `fujinet-nio`'s `docs/file_device_protocol.md`): whole pages of
entries with the total count, as many as fit the 512 byte transport. A screen
page in the standard build is two protocol pages of nine entries. It keeps 31
bytes of each name in the 800-byte page cache and fetches a full name when needed.
The selected full name is reused between its animation and opening the entry.

The banked build requests two names per protocol page, each up to the firmware's
220-byte listing limit. Nine protocol pages fill one 18-row screen. Their packed,
variable-length records are retained in an 8 KB cache in the Setup bank. The visible
rows keep short prefixes; scrolling and opening a longer name retrieve its full value
from that cache without another FujiNet request. When the cache fills it is cleared,
and the current screen's pages are gathered again before drawing. Both builds
invalidate the cache on entering a directory, changing filter or pressing R; R also
asks the device to re-read the folder. The maximum composed URI remains 196 bytes.

When an uncached page is requested, the current page and “Reading directory...”
stay visible in the body colour while the device replies and the next page is drawn
in the back buffer. The next vertical blank presents the completed page and restores
the row-colour interrupts together. A failed refresh retains the previous page and
shows the error. The interrupt also stays off during every SIO transfer because it
otherwise upsets serial timing.

## Drawing speed

The text routines are in assembly (`atari_scr_fast.s`): the C macros in
`atari_scr.h` store their arguments in zero page and call a routine that writes
screen codes straight to a row address table, with no C argument stack.
Numbers are formatted by subtraction against a power-of-ten table, not by
division. Directory sizes are turned into kilobytes once, when a page is
loaded, and dates use 16-bit day counts.

Everything is drawn into a back screen and shown with `scr_present()` during
vertical blank (the vblank handler swaps the display list's row addresses and
the row colours together), so a redraw appears in one frame instead of wiping
down the screen. `input_key()` presents before it waits. Moving the cursor
within a page recolours two rows; moving past the edge slides the screen rows
and draws the one new row.

Measured in the emulator (jiffies, 1 jiffy = 20 ms PAL): a full Browse page
34 to 6, Hosts 9 to 2, Drives 10 to 2; a cursor move 3. Build with
`EXTRA_DEFS=-DDRAW_TIMING` to read the last draw time from `dbg_draw` and
`dbg_move` (symbols in the `.lbl` file).

## Running it

Build (needs `cc65` and the Atari library; `scripts/env.sh` sets `CC65_HOME`):

```sh
make -f makefiles/build.mk TARGET=atari        # build/atari/bin/config-nio.xex
```

`scripts/atari_shot.py` runs the XEX in headless AltirraSDL with the embedded
FujiNet-NIO device (no window opens), replays keys and saves screenshots:

```sh
python3 scripts/atari_shot.py --xex build/atari/bin/config-nio.xex --out /tmp/shots \
    --script "wait:4 key:RETURN wait:2 shot:browse" --dump
```

The embedded device does not serve D1: disk SIO. To boot an ATR through the real
firmware (NetSIO hub + POSIX fujinet-nio + AltirraSDL), and to exercise
OPTION-to-boot, use `scripts/atari_shot_netsio.py` with a boot ATR (the disk
takes about 20 s to load there):

```sh
python3 scripts/atari_shot_netsio.py --atr boot.atr --out /tmp/shots --data demo.atr=some.atr \
    --script "wait:22 shot:first"
```

The automated suite (fast embedded tests, screenshot comparison, and the slow boot
tests for PicoBoot, BASIC, RESET and OPTION-to-boot) is described in
`RUNNING_TESTS.md`.

## Boot disk

`repos/nio-core-apps/boot-disk/manifests/atari.yaml` stages `config-nio.xex` as
`autorun.xex` on the default boot ATR (PicoBoot runs the first file).
`atari-ftestnio.yaml` builds the old hardware bring-up disk.
