# Amiga Config: change the RS-232 rate from the Configuration window

Date: 2026-11-24. Repos: `nio-config` (Config, boot disk profiles, docs) and
`fujinet-nio-driver` (`fujinet-nio-baud`). Workspace installer:
`configs/amiga/install/Install-FujiNet-WB13`.

## Why

Today the Amiga's serial rate is set by `C:fujinet-nio-baud <rate>`: by hand
at a shell, or by a line in the free boot disk's `S:Startup-Sequence`. The
free disk has no editor, so changing the rate for good means editing the ADF
on another machine. The user wants to change it from Config, have the change
survive a reboot, and not be left with an Amiga that cannot reach its FujiNet.

The FujiNet's own rate can only be changed on its serial console
(`uart.baud <rate>`, `uart.save`); there is no FujiBus command for it. So
Config can change only the Amiga half, and the user must change the FujiNet
half by hand while Config waits.

## Goals

- A rate setting in Config's Configuration window.
- Changing it switches the Amiga's rate at once, then waits for the user to
  set the FujiNet to match, checking the link until it answers.
- If the FujiNet answers at the new rate, the rate is saved and is used on
  every boot.
- If it does not answer in time, or the user cancels, Config switches back to
  the old rate and saves nothing (user's choice: revert, like a screen-mode
  "keep these settings?" prompt).
- Works on Kickstart 1.3 and 2.0+, on the free disk, the Workbench boot disks
  and hard-drive installs.

## Non-goals

- Changing the FujiNet's rate from the Amiga (needs new firmware protocol).
- Choosing the serial driver (`fujinet-nio-serial`) from Config. The settings
  file leaves room for a later `SERIAL=` key; it is not part of this work.
- Per-Kickstart rates.

## Anti-goals

- Config must never rewrite a file the Amiga boots from (`S:Startup-Sequence`,
  `S:User-Startup`, `S:StartupII`). A failed or interrupted save must never
  stop the Amiga booting.
- A change must never leave Config unable to reach the FujiNet unless the
  user confirmed the FujiNet answers at the new rate.

## Approaches considered

- **A. Config edits the startup file's `fujinet-nio-baud` line** (or adds
  one). Literal, visible in the startup file, but Config rewrites the boot
  file, must guess which file and work around custom ones; a bad write can
  leave the machine unbootable. Rejected for the anti-goal above.
- **B. The startup file reads the rate from a settings file Config owns.**
  **Chosen.** Config writes only a small settings file; the boot line never
  changes after it is added once.
- Saving the rate on the FujiNet (app store) was ruled out: at boot the Amiga
  must already be at the right rate to read it back.

## Design

### Settings file `S:FujiNet-Config`

Plain text, one `KEY=value` per line; lines starting with `;` are comments;
blank lines are ignored. Keys are upper case. This work defines one key:

```text
; FujiNet boot settings, written by Config
BAUD=38400
```

- `S:` is used rather than `ENVARC:` because Kickstart 1.3 and the free disk
  have no `ENVARC:`.
- Each boot tool reads only its own key. Unknown keys are ignored by readers
  and preserved by writers.
- Config saves by reading the existing file, replacing the `BAUD=` line (or
  appending one), copying every other line through unchanged, writing the
  result to `S:FujiNet-Config.new`, then replacing the old file with it.
  AmigaDOS `Rename()` does not overwrite, so: delete the old file, rename the
  new one. If the delete or rename fails, report it and leave whatever is
  there; a missing file is harmless (see below).

### `fujinet-nio-baud FROM=<file>`

New form in `fujinet-nio-driver/amiga/tools/fujinet-nio-baud.c`; the existing
`fujinet-nio-baud` and `fujinet-nio-baud <rate>` forms are unchanged.

- Reads `<file>`, finds the first `BAUD=` line, and sets that rate exactly as
  `fujinet-nio-baud <rate>` does.
- File missing: do nothing, print nothing, return 0 (leave the device's
  current rate). The boot must not stop or show an error.
- `BAUD=` missing, not a number, or outside 300..230400: print one warning
  line, set nothing, return 5 (WARN), so a startup script carries on.
- Every other failure in the `FROM=` form (cannot open `fujinet-nio.device`,
  `SET_BAUD` fails) also prints one line and returns 5 (WARN), never 20, so
  it can never stop a startup script at the default FailAt. The other forms
  keep returning 20 as today. (Builder-check call: the anti-goal outranks
  matching the old exit codes.)
- Must build for the bare-system/nix13 tool set (`make amiga-bootdisk-tools`)
  and the Workbench 3.x set, as today. Must work on Kickstart 1.3 DOS (no
  ReadArgs; parse `FROM=` by hand as the tool parses its rate today).

### Boot lines and shipped defaults

- **Free disk** (`amiga/bootdisk/profiles/free.toml`): replace
  `C:fujinet-nio-baud 38400` with `C:fujinet-nio-baud FROM=S:FujiNet-Config`,
  and ship `S/FujiNet-Config` containing `BAUD=38400`. The default rate stays
  38400; the user edits it from Config.
- **Workbench 2.0+ boot disk** (`wb204` profile `user_startup`): add the same
  line after the drivers load, and ship the same default file. Only add the
  line if `fujinet-nio-baud` is packaged on that disk; if it is not, package
  it.
- **WB 1.3 disk and hard-drive install** (`wb13` profile,
  `Install-FujiNet-WB13`): copy `fujinet-nio-baud` to `C:` (today the wb13
  package excludes it; add a nix13 build), copy a default `S:FujiNet-Config`
  only if none exists, and add the boot line to the lines the installer tells
  the user to put in `S:StartupII`.
- **WB 2.04 hard-drive installer** (`amiga/bootdisk/wb204/Install-HD`) and
  **Config installer** (`amiga/install/Install-config-nio`): copy
  `fujinet-nio-baud` to `C:`, copy a default `S:FujiNet-Config` only if none
  exists, and append the boot line to `S:User-Startup` where they already
  append the driver lines (Install-HD), or tell the user to add it
  (Install-config-nio, which does not edit startup files today).
- Existing installs keep working: without the boot line and file nothing
  changes.

The rate in the shipped files (38400) is a change for the Workbench disks,
which today never set a rate and so run at the driver's built-in 19200.
That is intended: 38400 is the rate verified on hardware.

### Config: the rate row and the change flow

- **Where:** the Configuration window's Device tab gets a row
  `Serial rate   38400` (read with `FUJINET_NIO_CMD_GET_BAUD`), and the
  Device tab's button row gets `Change rate...` next to `Refresh`.
- **Picking a rate:** `Change rate...` offers 9600, 19200, 38400, 57600 and
  115200 (the current one marked). Picking the current rate does nothing.
  (User's choice. Only 9600-38400 are verified on hardware; the large-reply
  check below is what keeps a marginal rate from being saved.)
- **Boot line check:** before switching, Config reads (never writes)
  `S:Startup-Sequence`, `S:User-Startup` and `S:StartupII`, whichever exist,
  looking for `fujinet-nio-baud` with `FROM=S:FujiNet-Config` (case
  insensitive). If none has it, the confirm requester says the rate would
  not be used at the next boot and offers "This session only" (switch and
  wait as below, but never save) or Cancel. This stops a saved rate the boot
  never applies from leaving the Amiga unable to reach a FujiNet already
  saved at the new rate.
- **Confirm:** a requester explains what happens:
  "Config switches the Amiga to 57600 now. Then, on the FujiNet's console,
  type: uart.baud 57600 and uart.save. Config waits 60 seconds and switches
  back to 38400 if the FujiNet does not answer." OK / Cancel.
- **Device access:** `fujinet-nio-lib` keeps its device handle private and
  has no baud API, so Config opens its own `fujinet-nio.device` handle for
  `GET_BAUD`/`SET_BAUD`, as `fujinet-nio-baud` does. No change to
  `fujinet-nio-lib`. (Builder-check call: keeps the work to two repos.)
- **Switch:** `FUJINET_NIO_CMD_SET_BAUD` with the new rate. The device closes
  its serial backend and the next exchange reopens it at the new rate.
- **Wait:** the window shows the console commands, a countdown and a Cancel
  button. Config repeats a cheap read-only FujiNet request (the same kind
  Refresh uses for the Firmware row) until one succeeds or 60 seconds pass.
  Once one succeeds, Config confirms the rate with a large read-only reply of
  about 500 bytes or more (for example a directory listing or app-store read
  sized to that), a few times in a row; any failure there counts as a
  failed change and reverts. Small replies pass at rates where large ones
  lose bytes, so a small reply alone does not prove a rate works.
  A failed request takes the driver's timeout (about 5 s at present), so
  expect roughly 10-12 tries; the countdown must still update and Cancel must
  still work between tries.
- **Success:** save `BAUD=<new>` to `S:FujiNet-Config` (above), refresh the
  Device tab, status "Serial rate 57600 saved". If the save fails, the rate
  stays in effect for this session and the status says it could not be saved
  (and why: disk write-protected, full, etc.).
- **Timeout or Cancel:** `FUJINET_NIO_CMD_SET_BAUD` with the old rate, check
  the link once, save nothing. Status: "No answer at 57600; back to 38400".
  If the link does not answer at the old rate either (the user changed the
  FujiNet but too late), say so and tell the user to set the FujiNet back to
  the old rate.
- **Help:** add the rate row, the flow and the FujiNet console commands to the
  Configuration help topic (`amiga_help.c`).

### Docs

`docs/amiga-bootdisk.md`: replace the "change the `fujinet-nio-baud` line"
text with the Config setting and `S:FujiNet-Config`, and document the file
format and the `FROM=` form. Driver README: document `FROM=`.

## Decided by the user

- Persist the change so it applies on every boot.
- Wait for the user to set the FujiNet, then on timeout go back to the old
  rate and save nothing.
- Store the rate in a settings file read at boot (approach B), named
  `S:FujiNet-Config`, generic `KEY=value` format, `BAUD=` only for now.
- Offer 9600, 19200, 38400, 57600 and 115200.

## Calls made in this design (not yet confirmed by the user)

- Missing settings file means "leave the rate alone", not "use 38400";
  instead each disk ships a default file. (Earlier in discussion this was
  described as "falls back to 38400"; this version keeps a file-less
  install unchanged.)
- The boot line check and its "This session only" choice.
- The large-reply confirmation before saving.
- The 60-second wait.
- `FROM=` returns WARN on every failure; Config opens its own device handle.
- The button label and placement, and the requester wording.
- Workbench disks move from 19200 to 38400 by default.

## Left to the builder

- Exact parsing code, file buffer sizes and error text.
- Which read-only request is used to check the link.
- How the countdown is drawn, within the existing Configuration window code.
- Host tests: parsing and the preserve-other-lines rewrite are pure functions
  and should get host unit tests alongside `tests/amiga/`; the boot disk
  tests (`tests/amiga/bootdisk/test_build.py`) should check the new boot line
  and shipped file for each profile.
