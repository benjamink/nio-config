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
| Browse | Directory listing of the chosen host. RET enters a directory (`..` and BS go up) or assigns the image under the cursor: 1-8 picks the drive, R toggles write protect. Opened from the Drives tab, the chosen image goes straight to that drive. |
| Drives | D1-D8 with their images. RET picks an image for the selected drive, X ejects, R toggles write protect. |
| Setup | Wi-Fi state, IP/gateway/DNS, MAC and firmware. RET scans for networks (or types an SSID) and joins. |

Keys: Ctrl+up/down (or `-` / `=`) move, SPACE and `b` page, TAB (or SELECT)
changes tab, **OPTION or START boots**. The joystick works too: stick moves,
the trigger is RET.

Each row has its own colours. A display list interrupt at the end of every row
(`atari_dli.s`) loads the next row's background and text colour from two 24
byte tables, so the title, selection bar, hint line and dialogs are whole-row
colour bars on a plain ANTIC mode 2 screen. A vertical-blank handler restarts
the row count every frame so a lost interrupt cannot leave the colours out of
step.

## Booting

OPTION (or START) mounts every assigned drive, unmounts D2-D8 that are not
assigned (D1 keeps the config disk until it is assigned), and cold starts the
Atari. The FujiNet keeps its mounts across the reset, so the Atari boots from
whatever is now in D1. DiskDevice slots are numbered from 1: D1 is slot 1.

XEX/COM programs are mounted like any image; the firmware presents them as a
boot disk (MyPicoDOS PicoBoot wrapper) so the same drive-assignment flow boots
them.

## Data

The app reads and writes the same records as the other front ends
(`docs/state-model.md`): the `hosts` text and the `mappings` v1 record in
the `config-nio` app-store namespace, and the slot catalogue (drive Dn uses
catalogue slot n-1). A drive map made on another machine type is read as is.

## Memory

The Atari is the tightest target: the portable layer (`src/platform/portable`)
does not fit, so the Atari has a compact core of its own (`atari_core.c`) and
does not use `fnsvc`, `fnctl` or conio.

* `makefiles/cc65-atari-nio.cfg` loads the program at `$0C00` (PicoBoot only needs
  the page below) up to `$BC00`.
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
* One 512 byte buffer serves the app store, slot catalogue and directory pages;
  host text and Wi-Fi scan results borrow the directory page buffer.

`ld65` fails the build if the program no longer fits, which is the memory check.

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

`tests/atari/test_ui_smoke.py` is an automated check of the tabs, host adding,
browsing, drive assignment, eject and write protect through the screen memory
(see `RUNNING_TESTS.md`).

## Boot disk

`repos/nio-core-apps/boot-disk/manifests/atari.yaml` stages `config-nio.xex` as
`autorun.xex` on the default boot ATR (PicoBoot runs the first file).
`atari-ftestnio.yaml` builds the old hardware bring-up disk.
