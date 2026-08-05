#!/usr/bin/env python3
"""
create_ssd.py - Create BBC Micro SSD disk images from source files

Usage:
    ./create_ssd.py -i <input_dir> -o <output.ssd> [-t title] [-a load_addr] [-e exec_addr]

Handles:
- .bas files: tokenized using basictool (load/exec &001900 / &008023), unless a sidecar
  <name>.inf exists (then copied as-is; metadata comes from the .inf)
- Pairs like `$.MARK` + `$.MARK.inf`: load/exec/lock from .inf; directory and leaf name
  parsed from the host filename (<1 char><dot><leaf>, `$` = root directory)
- Other files: copied with configurable load/exec (default same as create-ssd.sh)
- Host hidden dotfiles (names starting with a period, e.g. .gitignore) are ignored
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List, Optional, Tuple


def parse_hex_address(spec: str) -> int:
    """
    Parse a BBC-style load address from a string.
    Accepts: 0x1900, 1900, &1900, &001900 (hex, 6502 address space 0-0xFFFF).
    """
    s = spec.strip()
    if not s:
        raise ValueError("empty address")
    if s.lower().startswith("0x"):
        return int(s, 16)
    if s.startswith("&"):
        s = s[1:]
    return int(s, 16)


def format_bbc_address(addr: int) -> str:
    """Format as dfstool-style string, e.g. &001900 (matches create-ssd.sh)."""
    if addr < 0 or addr > 0xFFFF:
        raise ValueError(f"address out of 16-bit range: {addr:#x}")
    return f"&{addr:06X}"


def normalize_bbc_address_word(value: int) -> int:
    """DFS catalogue uses 16-bit load/exec; accept 32-bit style lines and take low word."""
    return value & 0xFFFF


def companion_inf_path(data_file: Path) -> Path:
    """Sidecar path: for `$.MARK` -> `$.MARK.inf` (append .inf to full filename)."""
    return data_file.parent / (data_file.name + ".inf")


def has_companion_inf(data_file: Path) -> bool:
    return companion_inf_path(data_file).is_file()


def is_hidden_host_file(name: str) -> bool:
    """True for Unix-style dotfiles (.gitignore, .DS_Store, etc.); not `$.NAME` (root dir)."""
    return name.startswith(".")


def parse_inf_line(inf_path: Path) -> Tuple[str, int, int, bool]:
    """
    Parse first non-empty line: <DFS name> <load hex> <exec hex> [L]

    Load/exec may be 4- or 8-digit hex (e.g. ffff0e00); low 16 bits are used.
    Returns (dfs_name, load_addr, exec_addr, locked).
    """
    text = inf_path.read_text()
    line = ""
    for ln in text.splitlines():
        ln = ln.strip()
        if ln:
            line = ln
            break
    if not line:
        raise ValueError(f"empty or unreadable .inf file: {inf_path}")

    parts = line.split()
    if len(parts) < 3:
        raise ValueError(
            f"{inf_path}: expected 'name load exec [L]', got: {line!r}"
        )

    dfs_name = parts[0]
    load_raw = parse_hex_address(parts[1])
    exec_raw = parse_hex_address(parts[2])
    load_addr = normalize_bbc_address_word(load_raw)
    exec_addr = normalize_bbc_address_word(exec_raw)
    locked = len(parts) > 3 and parts[3].upper().startswith("L")
    return dfs_name, load_addr, exec_addr, locked


_DFS_HOST_PREFIX_RE = re.compile(r"^(.)\.(.+)$")


def parse_dfs_directory_and_leaf(host_filename: str) -> Tuple[str, str]:
    """
    BBC DFS name on disk: one directory character, '.', then leaf (rest of name).

    The second character of the filename must be '.' so that names like FOO.BAS are
    not mistaken for directory F + leaf OO.BAS.

    If the pattern does not apply, directory is '$' (root) and the leaf is derived
    POSIX-style (strip last extension): e.g. foo.bas -> FOO, BINARY -> BINARY.

    Returns (directory, leaf) with leaf truncated to 7 characters, uppercased.
    """
    if len(host_filename) >= 3 and host_filename[1] == ".":
        m = _DFS_HOST_PREFIX_RE.match(host_filename)
        if m:
            directory = m.group(1)
            leaf_raw = m.group(2)
            leaf = truncate_filename(leaf_raw.upper())
            return directory, leaf

    if "." in host_filename:
        stem = host_filename.rsplit(".", 1)[0]
    else:
        stem = host_filename
    return "$", truncate_filename(stem.upper())


def extract_filename_from_bas(bas_file: Path) -> str:
    """
    Extract filename from first line of BAS file if it contains:
    REM filename: DESIRED_NAME

    Returns the extracted name or the file's stem if not found.
    """
    try:
        first_line = bas_file.read_text().split("\n")[0]
        if "filename:" in first_line.lower():
            parts = first_line.lower().split("filename:", 1)
            if len(parts) > 1:
                name = parts[1].strip().split()[0]
                return name
    except OSError:
        pass

    # Avoid Path.stem — breaks names like $.FOO (POSIX sees $.FOO as stem '$')
    name = bas_file.name
    if name.lower().endswith(".bas"):
        name = name[:-4]
    return name


def truncate_filename(name: str, max_len: int = 7) -> str:
    """Truncate filename to BBC Micro limit (7 chars)."""
    if len(name) > max_len:
        print(f"  Warning: Filename truncated to fit BBC format: {name[:max_len]}")
        return name[:max_len]
    return name


def staging_temp_name(directory: str, leaf: str) -> str:
    """Unique-ish name under the host FS (avoids clash between A.FOO and B.FOO)."""
    d = "R" if directory == "$" else directory
    return f"_{d}_{leaf}"


def process_bas_file(bas_file: Path, temp_dir: Path) -> Tuple[str, str, Path]:
    """
    Tokenize a BASIC file and return (directory, filename, tokenized_path).
    DFS directory comes from the host filename; leaf name from REM text or host
    (same rules as parse_dfs_directory_and_leaf on that string).
    """
    extracted_name = extract_filename_from_bas(bas_file)
    _, filename = parse_dfs_directory_and_leaf(extracted_name)
    directory, _ = parse_dfs_directory_and_leaf(bas_file.name)
    output_path = temp_dir / staging_temp_name(directory, filename)

    print(f"  Tokenizing: {bas_file.name} -> {directory}.{filename}")

    result = subprocess.run(
        ["basictool", "-2", "-t", str(bas_file), str(output_path)],
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        print(f"Error tokenizing {bas_file}:")
        print(result.stderr)
        sys.exit(1)

    return "$", filename, output_path


def process_data_file(data_file: Path, temp_dir: Path) -> Tuple[str, str, Path]:
    """
    Copy a data file as-is and return (directory, filename, copied_path).
    Uses global load/exec from manifest defaults (caller).
    """
    directory, filename = parse_dfs_directory_and_leaf(data_file.name)
    output_path = temp_dir / staging_temp_name(directory, filename)

    print(f"  Copying: {data_file.name} -> {directory}.{filename}")

    output_path.write_bytes(data_file.read_bytes())

    return directory, filename, output_path


def process_inf_paired_file(
    data_file: Path, temp_dir: Path
) -> Tuple[str, str, Path, str, str, bool]:
    """
    Copy bytes as-is; load/exec/lock come from companion .inf only.
    Returns (directory, leaf, path, load_bbc, exec_bbc, locked).
    """
    inf_path = companion_inf_path(data_file)
    dfs_name, load_i, exec_i, locked = parse_inf_line(inf_path)
    directory, filename = parse_dfs_directory_and_leaf(dfs_name)
    load_bbc = format_bbc_address(load_i)
    exec_bbc = format_bbc_address(exec_i)

    output_path = temp_dir / staging_temp_name(directory, filename)

    print(
        f"  .inf metadata: {data_file.name} -> {directory}.{filename} "
        f"load {load_bbc} exec {exec_bbc}{' locked' if locked else ''}"
    )

    output_path.write_bytes(data_file.read_bytes())

    return directory, filename, output_path, load_bbc, exec_bbc, locked


class FileManifestEntry:
    """One file entry for the JSON manifest."""

    __slots__ = (
        "file_name",
        "directory",
        "locked",
        "load_addr",
        "exec_addr",
        "content_path",
        "manifest_type",
    )

    def __init__(
        self,
        file_name: str,
        directory: str,
        locked: bool,
        load_addr: str,
        exec_addr: str,
        content_path: Path,
        manifest_type: str,
    ):
        self.file_name = file_name
        self.directory = directory
        self.locked = locked
        self.load_addr = load_addr
        self.exec_addr = exec_addr
        self.content_path = content_path
        self.manifest_type = manifest_type


def create_manifest(
    entries: List[FileManifestEntry],
    temp_dir: Path,
    disc_title: str,
    disc_size: int,
) -> Path:
    """Create JSON manifest for dfstool."""
    manifest_path = temp_dir / "manifest.json"

    files_json = []
    for e in entries:
        files_json.append(
            {
                "fileName": e.file_name,
                "directory": e.directory,
                "locked": e.locked,
                "loadAddress": e.load_addr,
                "executionAddress": e.exec_addr,
                "contentPath": str(e.content_path),
                "type": e.manifest_type,
            }
        )

    manifest = {
        "version": 1,
        "discTitle": disc_title,
        "discSize": disc_size,
        "bootOption": "none",
        "cycleNumber": 0,
        "files": files_json,
    }

    manifest_path.write_text(json.dumps(manifest, indent=2))
    return manifest_path


def create_ssd(
    input_dir: Path,
    output_ssd: Path,
    disc_title: str,
    disc_size: int,
    data_load_addr: str,
    data_exec_addr: Optional[str],
    show_manifest: bool = False,
):
    """Main function to create SSD disk image."""

    if not input_dir.is_dir():
        print(f"Error: Input directory '{input_dir}' does not exist")
        sys.exit(1)

    try:
        load_val = parse_hex_address(data_load_addr)
        load_bbc = format_bbc_address(load_val)
    except ValueError as e:
        print(f"Error: invalid --load-addr {data_load_addr!r}: {e}")
        sys.exit(1)

    if data_exec_addr is None:
        exec_bbc = load_bbc
    else:
        try:
            exec_val = parse_hex_address(data_exec_addr)
            exec_bbc = format_bbc_address(exec_val)
        except ValueError as e:
            print(f"Error: invalid --exec-addr {data_exec_addr!r}: {e}")
            sys.exit(1)

    for tool in ["basictool", "dfstool"]:
        result = subprocess.run(["which", tool], capture_output=True)
        if result.returncode != 0:
            print(f"Error: {tool} not found in PATH")
            sys.exit(1)

    all_files = sorted(
        [
            f
            for f in input_dir.iterdir()
            if f.is_file() and not is_hidden_host_file(f.name)
        ],
        key=lambda p: p.name.lower(),
    )

    paired_with_inf = sorted(
        [
            f
            for f in all_files
            if not f.name.lower().endswith(".inf")
            and has_companion_inf(f)
        ],
        key=lambda p: p.name.lower(),
    )

    bas_files = sorted(
        [
            f
            for f in all_files
            if f.suffix.lower() == ".bas"
            and not has_companion_inf(f)
            and not f.name.lower().endswith(".inf")
        ],
        key=lambda p: p.name.lower(),
    )

    other_files = sorted(
        [
            f
            for f in all_files
            if f.suffix.lower() != ".bas"
            and not f.name.lower().endswith(".inf")
            and not has_companion_inf(f)
        ],
        key=lambda p: p.name.lower(),
    )

    for p in all_files:
        if p.name.lower().endswith(".inf"):
            stem_key = p.name[:-4]
            expected_data = p.parent / stem_key
            if not expected_data.is_file():
                print(f"  Warning: orphan sidecar (no data file): {p.name}")

    if not paired_with_inf and not bas_files and not other_files:
        print(f"Error: No files found in '{input_dir}'")
        sys.exit(1)

    print("📄 Creating SSD disk image...")
    print(f"💡  Disc title: {disc_title}")
    print(f"💡  Input directory: {input_dir}")
    print(f"💡  Output SSD: {output_ssd}")
    print(f"💡  Non-BASIC load/exec (no .inf): {load_bbc} / {exec_bbc}")

    print(f"\nFound {len(paired_with_inf)} file(s) with companion .inf:")
    for f in paired_with_inf:
        print(f"  - {f.name}")

    print(f"\nFound {len(bas_files)} .bas file(s) to tokenize (no .inf):")
    for f in bas_files:
        print(f"  - {f.name}")

    print(f"\nFound {len(other_files)} other file(s) (no .inf):")
    for f in other_files:
        print(f"  - {f.name}")

    with tempfile.TemporaryDirectory() as temp_dir_str:
        temp_dir = Path(temp_dir_str)
        print(f"\nUsing temporary directory: {temp_dir}")

        entries: List[FileManifestEntry] = []

        if paired_with_inf:
            print("\nProcessing files with .inf sidecars (no tokenization)...")
            for data_file in paired_with_inf:
                d, fn, out_path, la, ea, locked = process_inf_paired_file(
                    data_file, temp_dir
                )
                entries.append(
                    FileManifestEntry(
                        fn,
                        d,
                        locked,
                        la,
                        ea,
                        out_path,
                        "other",
                    )
                )

        if bas_files:
            print("\nTokenizing BAS files...")
            for bas_file in bas_files:
                d, fn, output_path = process_bas_file(bas_file, temp_dir)
                entries.append(
                    FileManifestEntry(
                        fn,
                        d,
                        False,
                        format_bbc_address(0x1900),
                        format_bbc_address(0x8023),
                        output_path,
                        "basic",
                    )
                )

        if other_files:
            print("\nCopying data files...")
            for data_file in other_files:
                d, fn, output_path = process_data_file(data_file, temp_dir)
                entries.append(
                    FileManifestEntry(
                        fn,
                        d,
                        False,
                        load_bbc,
                        exec_bbc,
                        output_path,
                        "other",
                    )
                )

        print(f"\nCreating JSON manifest with {len(entries)} files")
        manifest_path = create_manifest(
            entries,
            temp_dir,
            disc_title,
            disc_size,
        )
        
        if show_manifest:
            print("\n=== Generated Manifest ===")
            print(manifest_path.read_text())
            print("=== End Manifest ===\n")

        print(f"\nCreating SSD disk image: {output_ssd}")
        result = subprocess.run(
            [
                "dfstool",
                "make",
                "--output",
                str(output_ssd),
                "--overwrite",
                str(manifest_path),
            ],
            capture_output=True,
            text=True,
        )

        if result.returncode != 0:
            print("Error: Failed to create SSD disk image")
            print(result.stderr)
            sys.exit(1)

    print(f"\nSSD disk image created: {output_ssd}")
    print("Files included:")
    for e in entries:
        print(f"  - {e.directory}.{e.file_name} ({e.manifest_type})")


def main():
    parser = argparse.ArgumentParser(
        description="Create BBC Micro SSD disk images from source files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s -i ./basfiles -o output.ssd
  %(prog)s -t MyDisc -i ./bin -o disk.ssd -a 0x8000
  %(prog)s -i ./mix -o disk.ssd -t Programs -a 0x1900 -e 0x2000

File handling:
  - .bas files are tokenized with basictool (load &001900, exec &008023), unless a
    sidecar .inf exists — then the file is copied unchanged and metadata is read
    from the .inf.
  - If <filename>.inf exists next to a file, load/exec/lock come only from that .inf
    (command-line -a/-e are ignored for that file). Host filename sets DFS directory
    and leaf: one character, '.', then name (e.g. $.FOO is root directory, FOO); if
    the pattern does not apply, directory is '$' and the leaf is the POSIX stem.
  - Other files use --load-addr and --exec-addr (default load 0x1900; if --exec-addr
    is omitted, execution address matches load), matching create-ssd.sh behaviour.
  - Filenames starting with a period (host hidden files like .gitignore) are skipped.
        """,
    )

    parser.add_argument(
        "-i",
        "--input-dir",
        type=Path,
        required=True,
        help="Input directory containing source files",
    )

    parser.add_argument(
        "-o",
        "--ssd",
        type=Path,
        required=True,
        help="Output SSD disk image file",
    )

    parser.add_argument(
        "-t",
        "--title",
        default="beeb",
        help='Disc title (default: %(default)s, same as create-ssd.sh)',
    )

    parser.add_argument(
        "-a",
        "--load-addr",
        default="0x1900",
        help="Hex load address for non-.bas files (default: %(default)s)",
    )

    parser.add_argument(
        "-e",
        "--exec-addr",
        default=None,
        help="Hex execution address for non-.bas files (default: same as --load-addr)",
    )

    parser.add_argument(
        "--disc-size",
        type=int,
        default=800,
        help="Disc size in KB for manifest (default: %(default)s)",
    )
    parser.add_argument(
        "--show-manifest",
        action="store_true",
        help="Print the generated manifest file to stdout",
    )

    args = parser.parse_args()

    create_ssd(
        args.input_dir,
        args.ssd,
        args.title,
        args.disc_size,
        args.load_addr,
        args.exec_addr,
        args.show_manifest,
    )


if __name__ == "__main__":
    main()
