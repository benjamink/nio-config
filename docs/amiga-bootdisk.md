# Bootable Workbench floppies with FujiNet NIO

`make amiga-bootdisk` turns your own Workbench floppy into a bootable disk
with the FujiNet NIO drivers, Config, a desktop ConfigNIO icon and an
`Install-HD` script that copies everything to a hard drive.

```sh
source "$NIO_WORKSPACE/scripts/env.sh"
scripts/amiga-artifacts wb32            # workspace: stage the NIO package
make amiga AMIGA_PROFILE=wb32           # build config-nio
make amiga-bootdisk WB=wb204 BASE_ADF=/path/to/amiga-os-204-workbench.adf
```

The result is `build/amiga-bootdisk/FujiNet-WB204-Boot.adf`. Variables:
`WB` (profile), `BASE_ADF` (required), `NIO_PKG` (default: the workspace's
staged `wb32` package), `CONFIG_NIO` (default `build/amiga/wb32/bin/config-nio`)
and `BOOTDISK_OUT`. `make test-amiga-bootdisk` runs the host tests, which use a
synthetic base disk and need only `xdftool` (amitools).

## Base disks

Workbench is licensed software and is never committed here. The builder checks
the base disk's volume name and SHA-256 against its profile and refuses a
different disk (`--allow-unknown-base` skips the checksum). It never modifies
the base ADF; it works on a copy and keeps the base's boot block.

## Profiles

Each Workbench release is a file in `amiga/bootdisk/profiles/`:

| Profile | Base disk | Status |
| --- | --- | --- |
| `wb204` | Workbench 2.0x floppy ("Workbench2.0"), Kickstart 2.04 | built |
| `wb13` | Workbench 1.3.3 floppy ("Workbench1.3"), Kickstart 1.3 | built; see below |
| `free` | none: built from scratch, no Workbench (Kickstart 1.3 and 2.0+) | built; see below |
| `wb31`, `wb32` | | not yet written |

A profile lists what to delete from the base disk to make room, lines to drop
from `S/Startup-Sequence`, the package files to add, the `S/User-Startup`
contents and the installer. The build fails (and removes its output) if a listed
path is missing, the filesystem check is not clean, or fewer than
`min_free_bytes` remain.

## What the wb204 disk contains

- `Devs/fujinet-nio.device`, `Devs/fujinet-disk.device`
- `C/fujinet-load-resident`, `C/fmount`, `C/fumount`, `C/fmountrestore`
- `Tools/config-nio`, a `ConfigNIO` icon on the Workbench desktop, and a
  `WBStartup/FujiNet-Config` entry so Config starts when Workbench comes up
  (delete that file and its `.info` from `WBStartup` to stop it)
- `S/User-Startup`: loads both devices at boot and runs `fmountrestore` in the
  background, so an absent FujiNet cannot stall the boot
- `Install-HD` (with an icon): `Execute SYS:Install-HD [volume:]` copies the
  files above to a Workbench hard drive (default `DH0:`) and appends the same
  lines to its `S:User-Startup`. It asks first and removes nothing.

To fit one 880 KB floppy the profile removes ARexx, speech, printer support
and a few utilities (Ed, Edit, Clock, Display, More, DiskCopy and others).

Add a Workbench release by writing a profile, then build it against a real base
disk and check it boots on that Kickstart (Amiberry or hardware).

## The freely distributable disk (`free`)

`make amiga-bootdisk WB=free` needs no base disk. It builds a new floppy that
contains only this project's own files and an original boot block, so it can
be shared. The same disk boots on every Kickstart from 1.3. The ROM supplies
exec, dos, intuition and graphics, and its shell runs the disk's
`S/Startup-Sequence`, which loads the FujiNet devices, selects the FujiNet
serial driver and runs `Tools/config-nio`.

A bare system differs from a Workbench one in ways that matter here, all
found by testing:

- **Stock `serial.device` is a Workbench file** (`Devs:serial.device`), not in
  ROM, so it is missing on this disk. `fujinet-nio.device` then cannot open its
  serial driver and Config reports "FujiNet error 1" (`FN_ERR_NOT_FOUND`). The
  disk loads the project's own `fujinet-serial.device` and selects it with
  `fujinet-nio-serial`.
- **clib2 programs crash** (`#80000003`) on a bare system, so the Workbench 3.x
  tool set cannot be used as built. `make amiga-bootdisk-tools` rebuilds
  `fmount`, `fumount` and `fujinet-nio-serial` against the 2.0 headers and links
  them with the libnix `nix13` runtime, as config-nio is (this uses the
  `AMIGA_COMPILE_CRT` setting in `nio-core-apps`). `fmount` and `fumount` pick
  their drive model at run time: the Kickstart 1.3 drives below, or the dynamic
  `DN0:`–`DN7:` drives on 2.0+.
- **Kickstart 1.3 `Execute()` needs `C:Run`**, a Workbench file, so config-nio
  runs `fmount`/`fumount` inside its own process there (see
  `docs/amiga-config.md`). That needs Config to run from a CLI, as the disk's
  `S/Startup-Sequence` starts it.
- **No `RAM:` or `T:` on Kickstart 1.3** (1.3 loads `RAM:` from
  `L:Ram-Handler`, a Workbench file). So the commands' output is not captured
  (a failed mount shows only its return code), `FMOUNT slot 0|1` cannot
  record its logical unit, and `FUMOUNT 0|1` asks for the drive name instead.

Verified in Amiberry with POSIX FujiNet on the serial port, using the same ADF
on both ROMs:
- Kickstart 1.3 (68000, OCS): Config lists the hosts. A Config SCRIPT run
  (insert, eject, insert on `DO0:`) ends `ok=7 err=0`, `Dir DO0:` lists the
  image, and `fumount DO0:` ejects it.
- Kickstart 2.04 (68000, ECS): a Config SCRIPT run (insert, eject, insert on
  `DN3:`) ends `ok=5 err=0`, `Dir DN3:` lists the image, and `fumount DN3:`
  ejects it.

Known limits:
- On Kickstart 1.3, only the OFS drives (`DO`/`HO`) work. The FFS drives need
  Commodore's `Mount` and `FastFileSystem`.
- Real hardware: the disk uses `fujinet-serial.device` (8N1, Paula UART) instead
  of the stock driver.
- `fmountrestore` is left off (a clib2 tool; saved drives are not remounted at
  boot).
- No hard-drive installer: the installer script relies on Workbench commands.
- The boot CLI has no commands beyond those on the disk.

## Kickstart 1.3 drives and the `wb13` disk

On Kickstart 1.3 the FujiNet drives have fixed names, each with its own
`fujinet-disk.device` unit: `DN0:`/`DN1:` (DD, FFS), `HN0:`/`HN1:` (HD, FFS),
`DO0:`/`DO1:` (DD, OFS) and `HO0:`/`HO1:` (HD, OFS). The 1.3 `fmount` builds
the DOS entry for the OFS drives (`DO`/`HO`) itself, using the ROM filesystem,
so they need no `C:Mount`. The FFS drives still go through `C:Mount` and
`L:FastFileSystem` (the static entries appended to `Devs:MountList`), which
are Commodore files.

`make amiga-bootdisk WB=wb13 BASE_ADF=.../workbench-1.3.3.adf` keeps the
user's Workbench 1.3.3 disk and adds the drivers, `fmount`/`fumount` and
Config, which starts with Workbench. The startup CLI stays open (no
`EndCLI`), because on 1.3 `Execute()` from Config stalls once that CLI has
ended.

For a disk with no Commodore files, use `free` (above).

### Commodore files each disk needs

| Disk | Commodore files used |
| --- | --- |
| `wb204`, `wb13` | The whole base Workbench disk (the user's own copy) |
| `wb13`, FujiNet part only | `C:Mount` and `L:FastFileSystem` for `DN`/`HN` drives only; `RAM:`/`T:` (from the Workbench startup) for logical-unit `FMOUNT 0\|1` |
| `free` (Kickstart 1.3 and 2.0+) | None |
