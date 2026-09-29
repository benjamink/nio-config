# config-nio for the Amiga Workbench

`config-nio` for the Amiga is a Workbench program. It configures FujiNet NIO
hosts, the disk-image catalogue and the `DN0:`–`DN7:` drives. It runs on
Kickstart/Workbench 1.3 and later, in one window on the Workbench screen.

## Feature map

| BBC / MS-DOS config | Amiga |
| --- | --- |
| Hosts: add, edit, delete, move, browse | **Hosts** page: list, `URI` field; Browse / Add / Replace / Remove / Move Up / Move Down |
| Browse a host, enter directories, assign a file to a slot, map it to a drive | **Browse** page: name, size (or `Drawer`) and date; Open / Parent / Refresh / **Mount…**. Mount lists the drives and what each holds; pick one, keep or untick `RO`, and press Mount (or Replace) |
| Slots: page through 0–255, edit, clear | **Catalogue** (Project menu): all 256 slots; Set / Clear / Mount… |
| Drive map and "Mount + Exit" | **Drives** page: what is mounted where; Eject runs `FUMOUNT drive` |
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
| `FMOUNT` | `SYS:C/fmount` | Command used by Mount |
| `FUMOUNT` | `SYS:C/fumount` | Command used by Eject |
| `SCRIPT` | none | Run commands from a file instead of waiting for input |
| `RESULT` | `RAM:config-nio.result` | Transcript file for `SCRIPT` |

Paths must not contain spaces.

Mount and Eject need `FMOUNT`/`FUMOUNT` at those paths and the resident
`fujinet-disk.device`. On a fresh system, run these commands from the
package (the `NIO:` share on the Amiberry profiles):

```text
Copy NIO:fmount NIO:fumount SYS:C/
Copy NIO:fujinet-disk.device DEVS:
NIO:fujinet-load-resident DEVS:fujinet-disk.device fujinet-disk.device
```

If a command fails, its last output line is shown in the status bar, for
example `FMOUNT failed for DN0: fmount: Unknown command (rc 10)`.

## Using the window

- Click a page button (Hosts / Browse / Drives), or press Tab / Shift-Tab,
  to change page. **Project ▸ Catalogue…** shows the catalogue.
- Click a row to select it. Double-click it, or press Return, to run the
  page's main action: browse a host, open a drawer, mount an image file,
  or edit a catalogue slot.

To mount an image:

1. On **Hosts**, double-click a host.
2. On **Browse**, open drawers until you find the image, then double-click
   it (or select it and press **Mount…**).
3. The list shows the drives and what is in each. The first empty drive is
   selected and `RO` is ticked. Pick a drive, untick `RO` for read/write if
   you need it, and press **Mount**. If the drive already holds a disk the
   button reads **Replace** and asks first. **Cancel** or Esc goes back.

config-nio puts the image in a catalogue slot for you. It reuses the slot
that already holds that image, otherwise the first empty one.

- The cursor keys move the selection. With Shift they move a page at a time;
  with Alt they jump to the top or bottom. Backspace goes to the parent
  drawer. Help shows About. Esc quits.
- Remove, Clear, Eject and Replace ask for confirmation first.
- **Project** menu: Catalogue… (Right-Amiga-C), About… (Right-Amiga-?),
  Quit (Right-Amiga-Q).

## Drives, FMOUNT and FMOUNTRESTORE

Mount and Eject run the standard `FMOUNT` and `FUMOUNT` commands. The
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
| `mount DRIVE ro\|rw` | Mount the selected image, as the Mount… button does |
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
