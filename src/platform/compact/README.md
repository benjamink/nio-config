# The compact launcher

The Hosts, Browse, Drives and Setup launcher for machines that have no room for the
portable layer (`src/platform/portable`): about 48 KB, no operating-system services,
and a text screen the program draws on itself. The Atari and the C64 run it.

| File | What it is |
|---|---|
| `compact_app.c` | The screen and the keys: the four tabs, the editors, the help, `main()` |
| `compact_core.c`, `compact_core.h` | The data and every FujiNet call: hosts, drives, listings, settings |
| `compact_compose.s`, `compact_view.s` | Hand-written 6502 for URI composition and the list views |

Nothing here names a machine. There is no `#ifdef __ATARI__` or `__C64__`, and a
change for one machine goes in that machine's folder (`src/platform/atari`,
`src/platform/c64`), never in this one. Both targets build every file here, so a change
here is a change to both.

## What a machine provides

Each target's folder holds the machine's screen, its keyboard and `compact_platform.h`.
The build puts that folder on the include path (`makefiles/compiler-cc65.mk`), and
`compact_app.c` includes `compact_platform.h` for all of it.

**Screen** (`<machine>_scr.h`)

- `SCR_COLS`, `SCR_ROWS`: 40 and 24. `SCR_JIFFY`: the low byte of the machine's frame clock.
- `scr_init`, `scr_coldstart`, `scr_theme`, `scr_text`, `scr_text_inv`, `scr_fill`,
  `scr_putc`, `scr_uint`, `scr_dec16`, `scr_scrollbar`, `help_rows`.
- The colour schemes `T_*` and the glyphs `G_*`. Text is ASCII; the screen code converts.
- The text that names the machine's keys: `UI_HINT_*`, `UI_INFO_DRIVES`, `UI_HELP_*`.
- Display hooks for machines whose display needs help: `scr_present`, `scr_sio_hook`,
  `scr_reading`, `scr_reading_on`, `scr_flat_on`, `scr_flat_off`. A machine that needs none
  defines them as nothing.

**Keyboard** (`<machine>_input.h`)

- `input_key`, `input_settle`, `input_set_idle`.
- The key codes `K_UP`, `K_DOWN`, `K_LEFT`, `K_RIGHT`, `K_ESC`, `K_TAB`, `K_BS`, `K_RET`,
  `K_HELP` and `K_BOOT`. `K_BOOT` is "mount the drives and restart", whatever key does it.

**Hooks** (`compact_platform.h`)

| | Where it runs | What it is for |
|---|---|---|
| `PLATFORM_INIT()` | First in `main()` | Machine state to set before the screen exists |
| `PLATFORM_FAIL_DETAIL()` | The failure screen, after the message | Extra detail the machine can show |
| `PLATFORM_KEY(k)` | The main key loop, for keys the launcher did not take | Keys only this machine has |
| `SCR_COPY_ROW_VARS`, `SCR_COPY_ROW(to, from)` | Scrolling the hosts list | Moving one row's text to another |

A hook a machine does not need is defined as nothing.

## Adding a machine

1. Make `src/platform/<machine>/` with the screen, keyboard and `compact_platform.h` above.
2. Add the target to `makefiles/targets.mk`, a linker configuration to `makefiles/`, and
   `CONFIG_NIO_EXTRA_SRC_DIRS_<machine> := $(SRC_DIR)/platform/compact` to
   `makefiles/build.mk`.
3. Add the machine to the `atari c64` list in `makefiles/compiler-cc65.mk` that puts its
   folder on the include path.
4. If the launcher needs something the machine cannot provide through the above, change the
   interface here, for every machine, and write it down in this file.

## Checking a change

`make TARGET=atari`, `make atari-banked` and `make TARGET=c64` must all build. A change
that should not alter the Atari or the C64 (a move, a rename, a refactor) leaves the
sha256 of every `.xex` and `.prg` unchanged. The Atari's tests (`tests/atari`) drive the
Atari build in a headless emulator; the banked build is only exercised by the boot tests
(`NIO_ATARI_BOOT=1`, see RUNNING_TESTS.md), so run those after anything that moves code.

The Atari's link order matters, for a reason not yet found. `makefiles/build.mk` fixes the
order of its assembly because the directory order builds, passes the fast tests and fails three
of the boot tests (see the comment there for what moves). Do not change it without running them.
