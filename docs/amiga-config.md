# config-nio for the Amiga Workbench

`config-nio` for the Amiga is a Workbench program. It configures FujiNet NIO
hosts, the disk images you mount, the `DN0:`–`DN7:` drives and the FujiNet's
Wi-Fi network. It runs on
Kickstart/Workbench 1.3 and later, in one window on the Workbench screen.

## Feature map

| BBC / MS-DOS config | Amiga |
| --- | --- |
| Hosts: add, edit, delete, move, browse | **Hosts** page: list of up to 64 hosts, `URI` field; Browse / Add / Replace / Remove / Move Up / Move Down |
| Browse a host, enter directories, assign a file to a slot, map it to a drive | **Browse** page: name, size (or `Drawer`) and date; Open / Parent / Refresh / **Mount…** / **Catalogue**. Mount lists the drives and what each holds; pick one, keep or untick `RO`, and press Mount (or Replace) |
| Slots: page through 0–255, edit, clear | **Catalogue** page: the occupied slots that FMOUNT mounts from; Mount… / Set / Clear |
| Drive map and "Mount + Exit" | **Drives** page: drive, mode, slot and image; Eject runs `FUMOUNT drive` |
| Preferences | **Settings** menu: date `YY-MM-DD`/`YY-DD-MM`, sizes Full/Compact |
| Wi-Fi / adapter info | **Settings ▸ Configure** opens the **Configuration** window: **Device** tab (firmware version, build profile) and **Network** tab (link state, SSID, signal, access point, IP, subnet, gateway, DNS, MAC); Refresh / **Join…** (scan, pick, passphrase) / Close |

The pages use the same model as the Shell commands, so the two can be used
side by side:

| Window | Shell |
| --- | --- |
| Mount… (Browse) | `FIN` into a slot (the one already holding the image, else the first empty one), then `FMOUNT slot drive RO\|RW` |
| Catalogue (Browse) | `FIN slot image`; pick the slot from the list of all 256 |
| Catalogue page | `FLS` |
| Catalogue ▸ Set / Clear / Mount… | `FIN slot image` / `FOUT slot` / `FMOUNT slot drive` |
| Drives page | `FDRIVE` |
| Eject | `FUMOUNT drive` (the image stays in its slot) |
| Configuration window / Join… | Wi-Fi service (`0xF3`) `GET_STATUS`, `GET_CONFIG`, `GET_ADAPTER_INFO`, `SCAN`, `SET_CONFIG` |

Every change is saved to the FujiNet as soon as you make it. For that
reason the window has no Save/Use/Cancel buttons.

## Starting it

From a Shell:

```text
config-nio [FMOUNT=path] [FUMOUNT=path] [SPLASH=ON|OFF] [SCRIPT=file] [RESULT=file]
```

From Workbench, double-click the `config-nio` icon. Its ToolTypes use the same
`KEY=value` form. Unknown ToolTypes are ignored, and so are ToolTypes in
brackets, such as `(FMOUNT=...)`.

| Option | Default | Meaning |
| --- | --- | --- |
| `FMOUNT` | `fmount` | Command used by Mount, resolved through the inherited AmigaDOS command path |
| `FUMOUNT` | `fumount` | Command used by Eject, resolved through the inherited AmigaDOS command path |
| `SPLASH` | `ON` | A small window with the logo and a line of progress while the program connects to the FujiNet and loads its state, shown for at least a second. `OFF` (or `NO`, `0`) skips it. It is never shown for a `SCRIPT` run |
| `SCRIPT` | none | Run commands from a file instead of waiting for input |
| `RESULT` | `RAM:config-nio.result` | Transcript file for `SCRIPT` |

Explicit `FMOUNT=` and `FUMOUNT=` command paths must not contain spaces.

Mount and Eject need `fmount`/`fumount` on the command path inherited by
config-nio, plus the resident `fujinet-disk.device`. An installer may put the
tools in `SYS:C`; a package-based setup can instead add its directory (such as
`NIO:`) to the command path. Set the `FMOUNT=` or `FUMOUNT=` ToolType only to
override that normal lookup with a specific command path. On a fresh system,
install the resident device, for example from the `NIO:` package share:

```text
Copy NIO:fujinet-disk.device DEVS:
NIO:fujinet-load-resident DEVS:fujinet-disk.device fujinet-disk.device
```

If a command fails, its last output line is shown in the status bar, for
example `FMOUNT failed for DN0: fmount: Unknown command (rc 10)`.

## Using the window

- Click a page button (Hosts / Browse / Catalogue / Drives), or press Tab /
  Shift-Tab, to change page.
- Click a row to select it. Double-click it, or press Return, to run the
  page's main action: browse a host, open a drawer, mount an image file,
  mount a catalogue slot, or open (or remount) a drive.
- Rows in Browse, Catalogue and the mount pickers start with a small glyph:
  a drawer, a disk image (ADF or HDF), a plain file or a floppy.
- The **Drives** page shows its eight drives as tiles, four across. A mounted
  drive is a raised tile with a solid floppy, its image name and `RO`/`RW`
  and slot; a drive whose image is saved but not mounted keeps the outline with a faded (dotted) body and says "not mounted"; an empty drive is pressed in with a dotted floppy; selection is shown by the fill colour and does not change the bevel. Click a tile to
  select it, double-click (or Return) to open it, and use the cursor keys to
  move around the grid. On a window too short for two rows of tiles the page
  falls back to the list, and the same applies if chip memory for the icons
  cannot be allocated. Mount, Remount and Eject run AmigaDOS commands that
  hold the window until they finish, so the status line first says what is
  starting ("Mounting hd.adf on DN1: ...") and, on the tiles, the affected
  drive shows its floppy half in or out with "Mounting..." or "Ejecting...". The glyphs and icons use only the screen's own pens, so
  they look right on the four-colour Workbench 1.3 screen as well as 2.x/3.x.

To mount an image:

1. On **Hosts**, double-click a host.
2. On **Browse**, open drawers until you find the image, then double-click
   it (or select it and press **Mount…**).
3. The list shows the drives and what is in each. config-nio asks the FujiNet
   what the image is (DD or HD, OFS or FFS) and selects the first empty drive
   that can take it, with `RO` ticked; the status line names the type and the
   suggested drive. Drives fixed to a media type are tagged `[DD FFS]`, or
   `(HD OFS only)` when the image cannot go there, and Mount refuses an
   unsuitable drive before running `FMOUNT`. If the firmware cannot inspect
   the image, no drive is excluded and the first empty drive is selected. Pick a drive, untick `RO` for read/write if
   you need it, and press **Mount**. If the drive already holds a disk the
   button reads **Replace** and asks first. **Cancel** or Esc goes back.

To put an image in the catalogue without mounting it, select it on
**Browse** and press **Catalogue**. The list then shows all 256 slots
with what each holds, the cursor on the slot that already holds the image,
otherwise the first empty one, and `RO` ticked. Move to the slot you want
and press **Add** (or double-click, or Return); **Cancel** or Esc goes
back. Typing a number jumps to that slot: `2` then `3` goes to slot 23, and
after a 2 second pause the next digit starts a new number. On a slot that holds a different image the button reads **Replace**
and asks first.

When mounting, config-nio puts the image in a catalogue slot for you, as `FIN` would. It
reuses the slot that already holds that image, otherwise the first empty
one, and the status line names the slot. The **Catalogue** page lists the
occupied slots (read a page at a time with range requests), so images you
mounted before can be mounted again from there. When all 256 slots are used,
Mount reports that the catalogue is full; clear some slots on the Catalogue
page.

- The cursor keys move the selection. With Shift they move a page at a time;
  with Alt they jump to the top or bottom. Backspace, or double-clicking the
  `..` row at the top of a drawer's listing, goes to the parent drawer. Help shows About. Esc quits.
- Remove, Clear, Eject and Replace ask for confirmation first.
- **Project** menu: About… (Right-Amiga-?), Quit (Right-Amiga-Q).
- **Settings** menu: date and size formats, and **Configure** (the
  Configuration window below).

## Large folders

Browse asks the FujiNet for a folder a page at a time (the `ListPages`
command, four entries per page). The device sorts the folder once, directories
first, and config-nio keeps a window of about 200 entries, loading another
window when you scroll outside it, so a folder can hold far more entries than
the window (the list has up to 65535 rows, including the `..` row in a
drawer). **Refresh** makes the FujiNet read the folder again; other navigation
reuses its cached listing. If a window load fails, entries are unavailable until
Refresh retries, preventing a partial window from pointing at the wrong files.
Names up to 220 bytes are retained; a name the device had to truncate cannot be
opened or assigned. With firmware that
does not support `ListPages`, config-nio reads the whole folder with the older
listing command and shows its first 200 entries ("Showing 200 of N entries").
`dump entries` in a script lists the current window.

## Configuration

**Settings ▸ Configure** opens the **Configuration** window over the main
window, which waits until it closes. It is read from the FujiNet when it
opens, and again with **Refresh** (or Return). It has two tabs (Tab switches
between them) and a **Close** button on every tab; Esc or the close gadget
also close it. **Help** shows the built-in **Configuration** help topic in
the window's list; **Back** (or Esc) returns. The main window's **Project**,
**Settings** and **Help** menus work in this window too: Quit quits
config-nio, Settings changes apply at once, and Help topics (and Contents)
open in this window.

The **Device** tab shows the FujiNet's **Firmware** version and build
**Profile** (e.g. `S3 + FujiBus over GPIO (e.g. RS232)`). The **Network** tab
shows:

| Row | Shows |
| --- | --- |
| Wi-Fi | Connected, Connecting, Disconnected, Failed to connect, or Off |
| Network | The saved SSID |
| Signal | A 4-bar icon and Excellent/Good/Fair/Weak (when connected): 4 bars from −55 dBm, 3 from −67, 2 from −75, 1 from −85. `SCRIPT` output keeps the dBm reading |
| Access point | BSSID of the access point in use |
| IP address, Subnet mask, Gateway, DNS server | IPv4 settings (when connected) |
| MAC address | The FujiNet's station MAC |
| Wi-Fi control | FujiNet (ESP32), Host computer, Simulated or Unavailable |

MAC address needs firmware with the Wi-Fi service's `GET_ADAPTER_INFO`
command, and Firmware and Profile need FujiDevice's `GetInfo`
(`fn_wifi_get_adapter_info()` and `fn_fuji_get_info()` in fujinet-nio-lib).
Each is read on its own: older firmware shows `Needs newer firmware` for what
it lacks, and the Device tab still works on a FujiNet without Wi-Fi.

To change network:

1. On the **Network** tab press **Join…**. The FujiNet scans, and the list
   becomes the network picker (**Join**, **Rescan**, **Other…**, **Cancel**,
   **Close**) showing each network's signal (bar icon and rating) and
   Open/Secured. The saved
   network, else the strongest, is selected. Hidden networks (no name) are
   not listed.
2. Select a network and press **Join** (or double-click, or Return). For a
   secured network a **Join Wi-Fi Network** window asks for the passphrase
   (8–64 characters); press **Join** or Return there, **Cancel** to go back.
   For the saved network, leave it empty to keep the stored passphrase. An
   open network needs no passphrase; joining one asks first when it replaces
   the current network.
3. For a hidden network press **Other…**: the window also has a
   **Network** field for its name. Return moves from Network to Passphrase.
   Leave the passphrase empty for an open network.
4. **Rescan** scans again; **Cancel** or Esc goes back to the Network tab.

Join saves the SSID and passphrase on the FujiNet (persisted), clears any
pinned BSSID, enables Wi-Fi and asks the FujiNet to reconnect. config-nio then
waits up to 10 seconds and reports `Connected to … address …`,
`Could not connect …` or `Still connecting …`. The passphrase is shown as you
type it, is wiped from memory once Join has sent it, and is never read back
from the FujiNet.

A FujiNet whose Wi-Fi is managed by its host computer (POSIX host mode) can
be viewed but not switched.

## Drives, FMOUNT and FMOUNTRESTORE

Mount and Eject run the standard `FMOUNT` and `FUMOUNT` commands. The
resident `fujinet-disk.device` stores successful mappings, so
`FMOUNTRESTORE` brings them back in a later session. After each command,
config-nio re-reads the stored mapping to decide whether it succeeded. On
Kickstart 1.3 this is the only reliable check, because `Execute()` does not
report a command's return code.

On Workbench 1.3 the drive names are the eight static MountList endpoints:
`DN0: DN1: HN0: HN1: DO0: DO1: HO0: HO1:` (units 0–7): DD/FFS, HD/FFS, DD/OFS
and HD/OFS, two of each. On Workbench 2.0 to 3.0 any `DN` drive takes any
supported image. On Workbench 3.1 a `DN` drive whose handler has started stays
fixed to that density and filesystem until reboot, so the picker tags it and
steers other media to another drive.

One config-nio binary runs on every Workbench from 1.3. It picks the drive
scheme from the Kickstart version: Kickstart 1.x uses the fixed endpoints
above, and 2.0 or newer shows the eight interchangeable `DN0:`–`DN7:` drives.
The 2.0+ features (drive windows, theme colours, mounted-state detection) are
used only when the OS has them.

`FMOUNT`, `FUMOUNT` and `fujinet-disk.device` still come in a Workbench 1.3 set
and a standard set. At startup config-nio runs `FMOUNT` with no arguments to
see which set is installed (the 1.3 usage line names `HN0:`) and refuses to
start if it does not suit the OS, telling you which set to install. If `FMOUNT`
cannot be found or its answer is unrecognised, config-nio starts and the first
mount reports the problem.

## SCRIPT language

Each line holds one command. Blank lines, and lines that start with `;`, are
ignored. Each command writes `OK` or `ERR <status>` to the transcript, and the
file ends with `SCRIPT DONE ok=N err=M`. The program returns 0 when no command
failed, and 5 otherwise.

| Command | Action |
| --- | --- |
| `page hosts\|browse\|catalogue\|drives` | Show a page |
| `host add URI`, `host edit URI`, `host remove`, `host up`, `host down`, `host select N` | Host list |
| `browse`, `select NAME`, `enter`, `parent`, `assign [SLOT] ro\|rw` | Browse the selected host; `assign` without a slot is the Catalogue button |
| `mount DRIVE ro\|rw` | Mount the selected image, as the Mount… button does |
| `slot set N URI ro\|rw`, `slot clear N` | Catalogue |
| `insert SLOT DRIVE ro\|rw`, `eject DRIVE` | Drives (`DRIVE` is a name like `DN0:`) |
| `wifi status` | Re-read the Configuration window's data and write `NET <label> <value>` per row (Network rows, then Firmware and Profile) |
| `wifi scan` | Open the Join picker; write `NETWORK N RSSI OPEN\|SECURED SSID` per listed network (hidden ones are left out) |
| `wifi connect N [PASSPHRASE]` | Join scanned network `N` (after `wifi scan`); the passphrase is the rest of the line, spaces included |
| `wifi join SSID [PASSPHRASE]` | Join any network by name, hidden ones included. Without a passphrase, the saved network keeps its stored one; another network is joined as open |
| `wifi cancel` | Leave the Join picker |
| `dump hosts\|entries\|drives\|catalogue\|network\|status`, `dump slot N` | Write state to the transcript (`dump catalogue` writes `SLOT N RO\|RW URI` per occupied slot; `dump network` re-reads and writes the `NET` rows) |
| `wait TICKS` | Pause (1/50 s) so the window can be inspected |
| `quit` | Stop |

## Developer notes

| Module | Role |
| --- | --- |
| `amiga_ctl.c` | Controller over the portable `config_nio` state and store |
| `amiga_script.c` | `SCRIPT=` interpreter |
| `amiga_net.c` | Configuration window rows and the Wi-Fi Join flow over `fn_wifi_*` |
| `amiga_list.c`, `amiga_layout.c`, `amiga_theme.c`, `amiga_input.c`, `amiga_format.c`, `amiga_options.c`, `amiga_drives.c` | Pure helpers |
| `amiga_gui.c` | Intuition window, gadgets, menus and rendering (V33 API) |
| `amiga_main.c`, `amiga_exec.c`, `amiga_stack.c` | Process start-up, command execution, stack size |

Everything except the last two rows is tested on the host with gcc, against
an in-memory fake of the fujinet-nio calls:

```sh
make test-amiga-host              # all tests
make test-amiga-host ONLY=layout  # one test
```

Build one Workbench profile. All profiles produce the same binary (compiled
against the 2.0 NDK and linked with the libnix `nix13` runtime, which starts on
Kickstart 1.3); the profile only names the output directory.

```sh
make amiga AMIGA_PROFILE=wb32
make amiga AMIGA_PROFILE=wb13
```

The logo in the window and the icon both come from
`images/config-nio-160x96x4.png`. `amiga/tools/mklogo.py` resamples it for
hires pixels, which are about twice as tall as they are wide: 84x27 masks
in `src/platform/amiga/amiga_logo_data.c` and 96x30 icon art in
`amiga/gfx/config-nio.icon.txt`. The emblem is drawn in the fill pen and
the lettering in the text pen, so it suits both the 1.3 and the 2.x+
palettes. To regenerate both, and then the icon:

```sh
make regen-amiga-icons
```

## Wi-Fi signal in the status line

When the FujiNet reports a Wi-Fi status, the main window's status line shows four signal bars left of the mounted-drive count (all dim when not connected). The status is read once at startup (not in scripted runs) and refreshed whenever the Configure window reads or joins a network; it is not polled. The cell is omitted when the FujiNet has no Wi-Fi status.

In Browse, files that are not disk images (.adf, .hdf, .hdd) are drawn in a quieter pen because they cannot be mounted. Folders and images look normal, and a selected row keeps its highlight. The dimming is a hint only: it does not block Mount.

The mount picker (Mount... from Browse or the Catalogue) shows the same drive tiles. A fixed-media drive names its media type on the tile; a drive that cannot take the image is drawn in a quieter pen. While a mount or eject runs, the tile shows "Mounting..." or "Ejecting..." under its floppy.
