#!/usr/bin/env python3
"""Build a bootable Workbench floppy with the FujiNet NIO drivers and Config.

    amiga/bootdisk/build.py PROFILE --base BASE.adf --pkg NIO_DIR \
        --config-nio BIN [--out DIR]

PROFILE is a name under amiga/bootdisk/profiles/ (wb204) or a path to a
profile .toml.  BASE.adf is the user's own licensed Workbench floppy (never
committed); NIO_DIR is the staged NIO package (scripts/amiga-artifacts
<profile> in the workspace); BIN is the built config-nio executable.

The builder copies the base disk, removes what the profile lists to make
room, adds the FujiNet files, a boot-time S:User-Startup, a desktop ConfigNIO
icon and the Install-HD installer, then runs a filesystem check and fails if
the disk is left with less spare room than the profile requires.  Needs
xdftool (amitools).
"""
import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tomllib

HERE = os.path.dirname(os.path.abspath(__file__))
AMIGA_DIR = os.path.dirname(HERE)
PROFILE_DIR = os.path.join(HERE, "profiles")


class BuildError(Exception):
    pass


def xdf(image, *args, capture=False):
    cmd = ["xdftool", image, *args]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        raise BuildError(
            f"{' '.join(cmd)} failed: {(res.stderr or res.stdout).strip()}")
    return res.stdout if capture else None


def listing(image):
    """Return (volume name, {path: is_dir}) for every entry on the disk."""
    entries = {}
    volume = None
    stack = []
    for line in xdf(image, "list", capture=True).splitlines():
        if line.startswith(("sum:", "data:", "fs:")):
            break
        indent = len(line) - len(line.lstrip(" "))
        name = line.strip().split()[0] if line.strip() else ""
        if not name:
            continue
        if volume is None:
            volume = name
            continue
        depth = indent // 2 - 1
        del stack[depth:]
        is_dir = " DIR " in line
        entries["/".join(stack + [name])] = is_dir
        if is_dir:
            stack.append(name)
    return volume, entries


def free_bytes(image):
    for line in xdf(image, "info", capture=True).splitlines():
        if line.startswith("free:"):
            return int(line.split()[3])
    raise BuildError("cannot read free space from xdftool info")


def load_profile(arg):
    path = arg if arg.endswith(".toml") else os.path.join(PROFILE_DIR, arg + ".toml")
    if not os.path.isfile(path):
        names = sorted(f[:-5] for f in os.listdir(PROFILE_DIR) if f.endswith(".toml"))
        raise BuildError(f"unknown profile {arg!r}; available: {', '.join(names)}")
    with open(path, "rb") as f:
        return tomllib.load(f)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def make_install_icon(workdir):
    sys.path.insert(0, os.path.join(AMIGA_DIR, "tools"))
    import mkinfo  # noqa: E402  (shared with the config-nio icons)

    grid = mkinfo.load_art(os.path.join(AMIGA_DIR, "gfx", "readme.icon.txt"))
    data = mkinfo.build_icon(grid, mkinfo.WBPROJECT, "C:IconX", 4096, [])
    mkinfo.check_icon(data, grid, mkinfo.WBPROJECT)
    out = os.path.join(workdir, "Install-HD.info")
    with open(out, "wb") as f:
        f.write(data)
    return out


# Original boot block code (no Commodore bytes): find dos.library's resident
# structure and return its init vector to exec's strap, which is all a DOS
# boot block has to do.  Placed at offset 12 of the 1024-byte boot area.
#   lea   dosname(pc),a1 ; jsr FindResident(a6) ; movea.l d0,a0
#   movea.l RT_INIT(a0),a0 ; moveq #0,d0 ; rts ; dosname: "dos.library"
BOOT_CODE = bytes.fromhex("43FA0010" "4EAEFFA0" "2040" "20680016" "7000" "4E75") \
    + b"dos.library\0"


def boot_block(fs_flag=0):
    """A bootable 1024-byte DOS boot block (fs_flag 0 = OFS, 1 = FFS)."""
    blk = bytearray(1024)
    blk[0:4] = b"DOS" + bytes([fs_flag])
    blk[8:12] = struct.pack(">L", 880)
    blk[12:12 + len(BOOT_CODE)] = BOOT_CODE
    total = 0
    for i in range(0, 1024, 4):
        total += struct.unpack(">L", blk[i:i + 4])[0]
        if total > 0xFFFFFFFF:
            total = (total & 0xFFFFFFFF) + 1
    blk[4:8] = struct.pack(">L", ~total & 0xFFFFFFFF)
    return bytes(blk)


def create_scratch_disk(out, profile):
    """A new formatted OFS floppy with our own boot block (no base disk)."""
    if os.path.exists(out):
        os.unlink(out)
    cmds = ["create", "+", "format", profile["volume"], "ofs"]
    for d in profile.get("dirs", []):
        cmds += ["+", "makedir", d]
    xdf(out, *cmds)
    with open(out, "r+b") as f:
        f.write(boot_block(0))


def build(profile, base, pkg, config_nio, out_dir, allow_unknown_base=False,
          tools=None):
    name = profile["name"]
    scratch = bool(profile.get("scratch"))
    checks = [("config-nio binary", config_nio)]
    if not scratch:
        checks.insert(0, ("base ADF", base))
    for label, path in checks:
        if not path or not os.path.isfile(path):
            raise BuildError(f"{label} not found: {path}")
    if not os.path.isdir(pkg):
        raise BuildError(f"NIO package directory not found: {pkg}")

    expected = profile.get("base_sha256")
    if not scratch and expected and sha256(base) != expected and not allow_unknown_base:
        raise BuildError(
            f"{base} is not the base disk the {name} profile was written for "
            f"(sha256 mismatch). Pass --allow-unknown-base to use it anyway.")

    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, profile["output"])
    if scratch:
        create_scratch_disk(out, profile)
    else:
        shutil.copyfile(base, out)
        os.chmod(out, 0o644)
    workdir = os.path.join(out_dir, f".{name}-work")
    shutil.rmtree(workdir, ignore_errors=True)
    os.makedirs(workdir)

    try:
        volume, entries = listing(out)
        if not scratch and volume != profile["base_volume"]:
            raise BuildError(f"base disk is {volume!r}, profile expects "
                             f"{profile['base_volume']!r}")

        missing = [p for p in profile.get("delete", []) if p not in entries]
        missing += [p for p in profile.get("delete_dirs", []) if p not in entries]
        if missing:
            raise BuildError("base disk lacks paths the profile removes "
                             f"(wrong Workbench version?): {', '.join(missing)}")
        for path in profile.get("delete", []):
            xdf(out, "delete", path)
        for path in profile.get("delete_dirs", []):
            xdf(out, "delete", path, "all")

        remove = {l.strip().lower() for l in profile.get("startup_remove", [])}
        if remove:
            seq = os.path.join(workdir, "Startup-Sequence")
            xdf(out, "read", "S/Startup-Sequence", seq)
            with open(seq, encoding="latin-1") as f:
                lines = f.read().split("\n")
            kept = [l for l in lines if l.strip().lower() not in remove]
            if len(kept) == len(lines):
                raise BuildError("S/Startup-Sequence has none of the lines "
                                 "the profile removes")
            with open(seq, "w", encoding="latin-1", newline="") as f:
                f.write("\n".join(kept))
            xdf(out, "delete", "S/Startup-Sequence")
            xdf(out, "write", seq, "S/Startup-Sequence")

        for key, dst in (("startup_sequence", "S/Startup-Sequence"),
                         ("user_startup", "S/User-Startup")):
            if key in profile:
                text = os.path.join(workdir, key)
                with open(text, "w", encoding="latin-1", newline="") as f:
                    f.write(profile[key])
                if dst in entries:
                    xdf(out, "delete", dst)
                xdf(out, "write", text, dst)

        def put(src, dst):
            if not os.path.isfile(src):
                raise BuildError(f"missing input file: {src}")
            parent = os.path.dirname(dst)
            if parent and parent not in listing_dirs:
                xdf(out, "makedir", parent)
                listing_dirs.add(parent)
            xdf(out, "write", src, dst)

        listing_dirs = {p for p, d in listing(out)[1].items() if d}
        for item in profile.get("package", []):
            root = pkg
            if item.get("from") == "tools":
                if not tools:
                    raise BuildError("profile needs --tools (the bare-system "
                                     "tool build); run make amiga-bootdisk")
                root = tools
            put(os.path.join(root, item["src"]), item["dst"])
        put(config_nio, profile["config_nio_dst"])
        icons = os.path.join(AMIGA_DIR, "icons")
        empty = os.path.join(workdir, "empty")
        open(empty, "wb").close()
        if profile.get("tool_icon"):
            put(os.path.join(icons, profile["tool_icon"]),
                profile["config_nio_dst"] + ".info")
        if profile.get("desktop_file"):
            put(empty, profile["desktop_file"])
            put(os.path.join(icons, profile["desktop_icon"]),
                profile["desktop_file"] + ".info")
        if profile.get("autostart_file"):
            put(empty, profile["autostart_file"])
            put(os.path.join(icons, profile["autostart_icon"]),
                profile["autostart_file"] + ".info")
        if profile.get("installer"):
            put(os.path.join(HERE, profile["installer"]), profile["installer_dst"])
            xdf(out, "protect", profile["installer_dst"], "+s")
            put(make_install_icon(workdir), profile["installer_dst"] + ".info")

        fsck = xdf(out, "fsck", capture=True)
        if "clean" not in fsck:
            raise BuildError(f"filesystem check failed: {fsck.strip()}")
        free = free_bytes(out)
        if free < profile.get("min_free_bytes", 0):
            raise BuildError(
                f"disk has only {free} bytes free, profile requires "
                f"{profile['min_free_bytes']}: remove more from the base disk")
    except BaseException:
        os.unlink(out)
        raise
    finally:
        shutil.rmtree(workdir, ignore_errors=True)
    return out, free


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("profile")
    ap.add_argument("--base", help="licensed Workbench base ADF (not needed for\n                    profiles that build from scratch)")
    ap.add_argument("--pkg", required=True, help="staged NIO package directory")
    ap.add_argument("--config-nio", required=True, help="built config-nio binary")
    ap.add_argument("--out", default="build/amiga-bootdisk", help="output directory")
    ap.add_argument("--tools", help="directory of bare-system (libnix) tools "
                    "for profiles whose package items say from = \"tools\"")
    ap.add_argument("--allow-unknown-base", action="store_true",
                    help="skip the base disk checksum check")
    a = ap.parse_args(argv)
    if shutil.which("xdftool") is None:
        print("error: xdftool (amitools) is required", file=sys.stderr)
        return 2
    try:
        out, free = build(load_profile(a.profile), a.base, a.pkg,
                          a.config_nio, a.out, a.allow_unknown_base, a.tools)
    except BuildError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    print(f"{out}: {free} bytes free")
    return 0


if __name__ == "__main__":
    sys.exit(main())
