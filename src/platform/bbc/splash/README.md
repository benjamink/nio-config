# BBC shortened-screen splash proof of concept

This directory contains a standalone 6502 splash application and BBC Mode 5
screen-data tooling. It is intentionally separate from the main `config-nio` link: the
loader remains small, displays the splash, loads `CONFNIO` at `&1900`, and
transfers control to it.

The loader loads compressed `SCREENZ` into a temporary buffer at `&5800`,
decompresses it directly into screen RAM at `&7100`, displays it as a
160-by-96 Mode 5 bitmap, waits for a key, and restores MODE 7 before returning.
Only 3,840 bytes of screen RAM are used:

```text
&6931              splash program
&5800..             temporary SCREENZ load buffer
&7100..&7FFF       expanded SCREEN (40 bytes x 8 rasters x 12 rows)
```

The chained `CONFIG` build restores MODE 7 after `CONFNIO` has loaded and before
transferring control to `&1900`. This lets `config-nio` retain its normal BBC
high-memory limit of `&7C00`; the splash CRTC is never used during application
startup.

The CRTC retains normal PAL frame timing. Register R6 limits the displayed
bitmap to 12 character rows, while R12/R13 point the display at `&7100 / 8 =
&0E20`. The unused part of the frame is border rather than allocated screen
memory. `SCREENZ` is loaded and expanded with an all-black palette, and only
then is the visible palette installed. This prevents both MODE 7 teletext
garbage and partially expanded bitmap data from being displayed.

## Default build (no workspace tools)

Checked-in assets in this directory are enough for a normal clone to assemble
the splash program and build a DFS disk:

- `SCREENZ` — compressed Mode 5 bitmap packaged on the disk
- `SCREEN` — uncompressed gold image (tests / regen verification)
- `palette.inc` — Video ULA table included by `main.s`

SSD creation uses `scripts/create_ssd.py` inside the `nio-config` repo (needs
`basictool` / `dfstool` on `PATH`).

```sh
make -C src/platform/bbc/splash
make -C src/platform/bbc/splash disk
```

## Regenerating screen assets

When `images/config-nio-160x96x4.png` (or the palette) changes, rebuild the
checked-in assets with the workspace helpers:

```sh
make -C src/platform/bbc/splash regen-screen
```

That runs `scripts/bbc-image` and `scripts/compress` from the workspace root,
then updates `SCREEN`, `SCREENZ`, and `palette.inc` in this directory. Override
the source or palette if needed:

```sh
make -C src/platform/bbc/splash regen-screen \
  SCREEN_INPUT="$PWD/images/config-nio-160x96x4.png" \
  SCREEN_PALETTE="black,green,yellow,white"
```

Commit the updated assets after regenerating.

## Running

The outputs are placed under `build/bbc/splash/`. The disk can be mounted and
the proof run with:

```text
*SPLASH
```

The Beebium integration test verifies the expanded screen bytes, CRTC registers,
Video ULA mode and palette, rendered frame, and restoration to MODE 7 after a
key. Run it from the repository root with:

```sh
./integration-tests/beebium/run_pytest.sh \
  test_splash_bbc.py::test_bbc_splash_loads_short_mode5_screen_and_restores_mode7 -q
```
