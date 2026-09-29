# config-nio for the Amiga Workbench

`config-nio` for the Amiga is a Workbench program. It configures FujiNet NIO
hosts, the disk-image catalogue and the `DN0:`–`DN7:` drives. It runs on
Kickstart/Workbench 1.3 and later, in one window on the Workbench screen.

## Feature map

| BBC / MS-DOS config | Amiga |
| --- | --- |
| Hosts: add, edit, delete, move, browse | **Hosts** page: list, `URI` field; Browse / Add / Replace / Remove / Move Up / Move Down |
| Browse a host, enter directories, assign a file to a slot | **Browse** page: name, size (or `Drawer`) and date; Open / Parent / Refresh / Assign, using the `Slot` field and `RO` box |
| Slots: page through 0–255, edit, clear | **Catalogue** page: all 256 slots; selecting one loads it into `URI`, `Slot` and `RO`; Set / Clear |
| Drive map and "Mount + Exit" | **Drives** page: Insert runs `FMOUNT slot drive RO/RW`, Eject runs `FUMOUNT drive`, immediately |
| Preferences | **Settings** menu: date `YY-MM-DD`/`YY-DD-MM`, sizes Full/Compact |

Every change is saved to the FujiNet as soon as you make it. For that
reason the window has no Save/Use/Cancel buttons.

## Starting it

From a Shell:

```text
config-nio [FMOUNT=path] [FUMOUNT=path] [SCRIPT=file] [RESULT=file]
```

From Workbench, double-click the `config-nio` icon. Its ToolTypes use the same
`KEY=value` form. Unknown ToolTypes are ignored, and so are ToolTypes in
brackets, such as `(FMOUNT=...)`.

| Option | Default | Meaning |
| --- | --- | --- |
| `FMOUNT` | `SYS:C/fmount` | Command used by Insert |
| `FUMOUNT` | `SYS:C/fumount` | Command used by Eject |
| `SCRIPT` | none | Run commands from a file instead of waiting for input |
| `RESULT` | `RAM:config-nio.result` | Transcript file for `SCRIPT` |

Paths must not contain spaces.

## Using the window

- Click a page button, or press Tab / Shift-Tab, to change page.
- Click a row to select it. Double-click it, or press Return, to run the
  page's main action: Browse a host, open a drawer, edit a catalogue slot,
  or insert into a drive.
- The cursor keys move the selection. With Shift they move a page at a time;
  with Alt they jump to the top or bottom. Backspace goes to the parent
  drawer. Help shows About. Esc quits.
- Remove, Clear and Eject ask for confirmation first.
- **Project** menu: About… (Right-Amiga-?), Quit (Right-Amiga-Q).

## Drives, FMOUNT and FMOUNTRESTORE

Insert and Eject run the standard `FMOUNT` and `FUMOUNT` commands. The
resident `fujinet-disk.device` stores successful mappings, so
`FMOUNTRESTORE` brings them back in a later session. After each command,
config-nio re-reads the stored mapping to decide whether it succeeded. On
Kickstart 1.3 this is the only reliable check, because `Execute()` does not
report a command's return code.

On Workbench 1.3 the drive names are the eight static MountList endpoints:
`DN0: DN1: HN0: HN1: DO0: DO1: HO0: HO1:` (units 0–7).

## SCRIPT language

Each line holds one command. Blank lines, and lines that start with `;`, are
ignored. Each command writes `OK` or `ERR <status>` to the transcript, and the
file ends with `SCRIPT DONE ok=N err=M`. The program returns 0 when no command
failed, and 5 otherwise.

| Command | Action |
| --- | --- |
| `page hosts\|browse\|catalogue\|drives` | Show a page |
| `host add URI`, `host edit URI`, `host remove`, `host up`, `host down`, `host select N` | Host list |
| `browse`, `select NAME`, `enter`, `parent`, `assign SLOT ro\|rw` | Browse the selected host |
| `slot set N URI ro\|rw`, `slot clear N` | Catalogue |
| `insert SLOT DRIVE ro\|rw`, `eject DRIVE` | Drives (`DRIVE` is a name like `DN0:`) |
| `dump hosts\|entries\|drives\|status`, `dump slot N` | Write state to the transcript |
| `wait TICKS` | Pause (1/50 s) so the window can be inspected |
| `quit` | Stop |

## Developer notes

| Module | Role |
| --- | --- |
| `amiga_ctl.c` | Controller over the portable `config_nio` state and store |
| `amiga_script.c` | `SCRIPT=` interpreter |
| `amiga_list.c`, `amiga_layout.c`, `amiga_theme.c`, `amiga_input.c`, `amiga_format.c`, `amiga_options.c`, `amiga_drives.c` | Pure helpers |
| `amiga_gui.c` | Intuition window, gadgets, menus and rendering (V33 API) |
| `amiga_main.c`, `amiga_exec.c`, `amiga_stack.c` | Process start-up, command execution, stack size |

Everything except the last two rows is tested on the host with gcc, against
an in-memory fake of the fujinet-nio calls:

```sh
make test-amiga-host              # all tests
make test-amiga-host ONLY=layout  # one test
```

Build one Workbench profile. `wb13` uses the nix13 CRT; `wb31` and `wb32`
use clib2.

```sh
make amiga AMIGA_PROFILE=wb32
make amiga AMIGA_PROFILE=wb13
```

To regenerate the checked-in icon from `amiga/gfx/config-nio.icon.txt`:

```sh
make regen-amiga-icons
```
