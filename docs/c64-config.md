# C64 config

`config-nio` for the C64 is the Atari's launcher (`src/platform/atari/atari_app.c` and
`atari_core.c`, unchanged in what they do) over the C64's own screen and keyboard in
`src/platform/c64/`. [The Atari's document](atari-config.md) describes the tabs, the data
it keeps and how booting works; this one covers what differs.

Run it from the boot disk with `LOAD"*",8,1` and `RUN`.

## Keys

| Key | Does |
|---|---|
| Cursor keys, or a joystick in port 2 | Move |
| RETURN, or fire | Choose |
| F1 | Mount the assigned drives and restart the machine |
| F3 or HOME | Change tab |
| F5 | Run the FujiNet demo from the boot disk |
| F7 or H | Help |
| RUN/STOP or the left-arrow key | Cancel |
| DEL | Up a level, and erase when editing |
| C= and @ | Underscore, in a URI |

## Screen

Text is on the VIC-II's own screen in the lower/upper case character set, since URIs are
case sensitive. The app uses the top 24 of its 25 rows. The VIC-II has one background
colour, so the Atari's coloured row bars (title, selection, errors) are reverse video here.

## Strings

The app and the library compile with no cc65 target (`-t none`, with `__C64__` and
`__CBM__` defined by hand) and link as a c64 program. cc65's `c64` target would turn every
string literal into PETSCII, and everything that crosses FujiBus is ASCII. The strings stay
in ASCII, and only `c64_scr.c` and `c64_input.c` convert: to screen codes, and from PETSCII.

## Drives

D1-D8 in the app are devices 8-15. DiskDevice slot 1 is device 8.

## Memory

The program loads at `$0801` with cc65's usual BASIC stub, and the startup code banks BASIC
out, which leaves up to `$CFFF`. It takes about 40 KB of the 51 KB there
(`makefiles/cc65-c64-nio.cfg`, and `build/c64/bin/config-nio.map`). The app's assembly passes
arguments in zero page, which is why the configuration gives it BASIC's. The app never
returns to BASIC as it found it: F1 leaves through the reset vector.

## The demo

F5 loads and runs `FUJIDEMO` from drive 8, which is the boot disk until F1 mounts something
else. The demo's sources and build are in `repos/nio-core-apps`. The program loads over the
app, so `src/platform/c64/c64_run.s` copies itself to the cassette buffer, sets the KERNAL and
BASIC up again, and uses BASIC's own steps to load and run it. If the file isn't there, the
result is the usual `?FILE NOT FOUND ERROR` and `READY.`, and `LOAD"*",8,1` gets back to the app.
