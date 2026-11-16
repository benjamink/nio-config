# config-nio for the TRS-80 Model 4/4P — design

Status: draft for approval. Items marked **(my call)** were decided by the author of this
document, not by the project owner; check them.

## Intent

A full-screen `CONFIG/CMD` for LS-DOS 6.3.1 on the Model 4/4P, equivalent in function to the
MS-DOS `config-nio`: manage hosts, browse FujiNet storage, assign images to drive slots, and
change Wi-Fi/preferences. It ships on the boot disk built in `~/dev/boot-trs80-4p`
(`tools/make_boot_image.py`), alongside `FMOUNT`, `FUMOUNT`, `FDRIVE`, `FLS` and `FUJIDRV`.

Why: today the 4P is driven by command-line tools. A launcher/config screen makes the FujiNet
usable without remembering URIs and drive numbers, and keeps the 4P consistent with the other
platforms.

## Goals

- Same four areas as MS-DOS: **Hosts**, **Browse** (files left, drive slots right), **Slots**,
  **Setup** (Wi-Fi and preferences).
- Reads and writes the shared records (hosts text, `mappings` v1, slot catalogue, preferences in the
  `config-nio` app-store namespace) exactly per `docs/state-model.md`, so state made on another
  platform loads unchanged.
- ASCII-only 80x24 text. Directory sizes, scrolling and the filter work as on MS-DOS where the
  firmware supports them.
- Written in **C** (decided by owner, for maintainability), reusing `src/platform/portable/` logic
  where it compiles for the Z80 **(my call on how much is reused; see Open points)**.

## Non-goals

- 64x16 mode, Model I/III/4 floppy-only variants, other DOSes (TRSDOS 6, NEWDOS, CP/M).
- Changing the wire protocol, `nioclib.asm` behaviour, or the boot loader.
- Graphics/inverse-video tricks beyond what plain 4P text mode gives (reverse video is optional).
- DMK/JV3 image support (FujiNet NIO does not have it yet).

## Anti-goals

- Hand-maintained assembly for UI or state logic. Assembly stays confined to the existing
  transport (`nioclib.asm`) and a small C-callable glue layer.
- A UI that behaves differently from MS-DOS for the same key where a 4P equivalent exists.

## Navigation (decided)

Matches MS-DOS: letters are actions, arrows move, ENTER acts, TAB switches pane focus.

| Key | Action |
|---|---|
| Up / Down | move selection |
| Left / Right | previous / next tab, or page at list edges **(my call)** |
| SHIFT+Up / SHIFT+Down | page up / down (4P has no PgUp/PgDn) |
| ENTER | open folder, mount selected image, accept |
| TAB | switch Files / Slots pane focus |
| BREAK | cancel dialog, go back, quit from top level (stands in for MS-DOS ESC; the 4P has no ESC) |
| Q | quit, as on MS-DOS |
| H / B / S / W | jump to Hosts / Browse / Slots / Setup **(my call on letters)** |
| A / E / D | add / edit / delete host or slot entry |
| X | eject slot |
| R | reload folder |
| F | name filter (`*` and `?` wildcards, as on Atari/MS-DOS) |
| ? | help for the current tab **(my call)** |

Text fields (URI, SSID, filter) take every letter; BREAK cancels, ENTER accepts, BACKSPACE
deletes, CTRL-A/E/K as MS-DOS for home/end/kill if the key codes exist. F1-F3 are unused.

## Constraints

- Toolchain: z88dk (SCCZ80 or SDCC backend) for C; `sjasmplus` for existing assembly. z88dk is not
  installed yet; the owner will install it. **(my call: the first task is a "hello" `/CMD` built with
  z88dk and run in MAME, to prove the toolchain and load format before porting anything.)**
- Output must become an LS-DOS `/CMD` load module (the boot repo's `trs80/lmf.py` already writes
  those) placed on the boot image by `tools/make_boot_image.py`.
- Memory: LS-DOS user programs run from 3000h; the existing tools use 8000h (client entry table),
  9F00h-A5FFh (buffers) and the driver at E000h with HIGH$ at DFFFh. The program, its C data and stack
  must fit without overlapping those **(my call: reuse those buffers and the 8000h entry table
  rather than link a second copy of `nioclib`)**. Size is the main risk; if the C does not fit,
  Browse/Slots data is paged from the FujiNet instead of cached (as the Atari and BBC do).
- Interrupts are off while talking to the FujiNet (existing behaviour of every 4P tool), so the
  screen is polled, not interrupt-driven.
- Transport: 19200 baud 8O1 on the 4P UART through `nioclient` calls (`nio_request`, `disk_*`,
  `file_list`).
- Mounting requires `FUJIDRV :d` on that drive; CONFIG must install or check it (as `FMOUNT`
  documents) rather than assume it.
- Repo rules: sources in `nio-config/trs80/` and Z80 platform code in `src/platform/trs80/`
  **(paths agreed by owner)**. Per workspace rules, commit per story, never push, no
  `Co-authored-by`. A change to the shared state or protocol would also need the firmware Python
  client updated; this work should not need that.

## Architecture (my call)

```
 UI (C): tabs, lists, dialogs, key handling        src/platform/trs80/config_nio_ui.c
 portable state/tables/browse (C, shared)          src/platform/portable/*
 fnsvc/store adapter (C): request builders over    src/platform/trs80/fnsvc_trs80.c
   nio_request, list directory, app-store get/put
 glue (asm): C-callable wrappers for the 8000h     src/platform/trs80/nio_glue.asm
   entry table, ROM-free console (video RAM,
   keyboard matrix/KBD DCB), UART
 existing nioclib.asm (boot repo)                  unchanged
```

Console I/O goes through LS-DOS (`@DSP`/`@KBD` SVCs) rather than touching hardware, so it also runs
in MAME and survives LS-DOS changes **(my call)**.

## Testing

Cheapest failing test per layer, per `docs/agent-test-policy.md`:

- Host-side: unit tests for any shared C logic built with gcc (like `make test-amiga-host`).
- Z80: the boot repo's Z80 interpreter tests with the fake NIO (`tests/fakenio.py`) for the
  transport glue and fnsvc adapter.
- End to end: MAME `trs80m4p` (`tools/mame4p.py`) boots the image, runs `CONFIG`, drives the keys,
  and asserts the screen text and a mount against the fake NIO. Real-4P verification is recorded
  as pending until the owner runs it.

## Staging (my call)

1. Toolchain proof: z88dk `/CMD` that prints text and reads a key, in MAME.
2. Console layer and tab/list widgets with fake data.
3. Transport adapter: list a directory, read host/mapping state from the fake NIO.
4. Browse + mount/eject + Slots.
5. Hosts add/edit/delete, persistence.
6. Setup: Wi-Fi scan/join and preferences.
7. Boot image integration, docs (`docs/trs80-config.md`, README table), real-4P check.

## Open points for the builder

- How much of `src/platform/portable/` compiles on z88dk and fits in memory; decided in stage 2.
- Exact key codes the 4P and LS-DOS `@KBD` return for arrows, SHIFT+arrows and BREAK
  (verify in MAME and on hardware).
- Whether the FujiNet app-store/Wi-Fi commands used by MS-DOS are all reachable through the 4P's
  serial FujiBus (expected yes; same device protocol).
- Whether a `fujinet-nio-lib` TRS-80 target should replace the in-repo adapter later
  (not in scope now).
