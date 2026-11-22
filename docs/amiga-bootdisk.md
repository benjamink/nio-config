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
| `config-only` | none: built from scratch, no Workbench | built; see below |
| `wb13`, `wb31`, `wb32` | | not yet written |

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

## The freely distributable disk (`config-only`)

`make amiga-bootdisk WB=config-only` needs no base disk. It builds a new floppy
that contains only this project's own files (drivers, `fmount`/`fumount`,
config-nio) and an original boot block, so it can be shared. The Kickstart ROM
in the Amiga supplies exec, dos, intuition and graphics; the disk's
`S/Startup-Sequence` loads the two FujiNet devices and runs `Tools/config-nio`.

Verified in Amiberry: it boots on Kickstart 1.3 and 2.04 with no Workbench, and
config-nio opens (reporting "FujiNet error 1" when no FujiNet is attached).

Not verified, and known limits:
- config-nio with a FujiNet attached on this disk, including mounting (it runs
  `fmount` through `SystemTags()`, which has not been shown to work without a
  Workbench startup).
- `fmountrestore` is left off: it crashed the bare system when no FujiNet
  answered, so saved drives are not remounted at boot.
- It uses the Workbench 3.x tool set (`wb32`), so it targets Kickstart 2.0+.
  Kickstart 1.3 needs the `wb13` tool set and its own profile.
- No hard-drive installer: the installer script relies on Workbench commands.
