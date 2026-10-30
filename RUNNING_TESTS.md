# Running nio-config tests

The repository currently has four practical test layers:

* target builds, which catch C/ASM, linker, and memory-limit regressions;
* BBC Beebium integration tests, which exercise `CONFNIO` through the BBC UI,
  fn-rom, and a real fujinet-nio PTY process;
* Atari UI smoke tests, which drive the Atari XEX in headless AltirraSDL and
  assert on its screen memory (`tests/atari`);
* Amiga host tests, which exercise the Amiga controller and helpers with gcc
  against an in-memory fake of the fujinet-nio calls.

The commands below assume the normal workspace layout:

```text
workspace/
  repos/nio-config/
  repos/fujinet-nio/
  repos/fujinet-nio-lib/
  repos/fn-rom/
```

Run the commands from `repos/nio-config` unless noted otherwise.

## Quick build checks

Build the Linux target:

```sh
make TARGET=linux FUJINET_NIO_LIB=../fujinet-nio-lib
```

Build the Atari target (a link failure here means the program no longer fits
the Atari memory layout):

```sh
make TARGET=atari FUJINET_NIO_LIB=../fujinet-nio-lib
```

Build the BBC target (including the 8-bit memory/linker check):

```sh
make TARGET=bbc FUJINET_NIO_LIB=../fujinet-nio-lib
```

Build the BBC Master target:

```sh
make TARGET=master FUJINET_NIO_LIB=../fujinet-nio-lib
```

Build the DOS target (the usual workspace setup provides a helper script for
the OpenWatcom environment):

```sh
# you must ensure watcom is on your path
make TARGET=msdos FUJINET_NIO_LIB=../fujinet-nio-lib
```

To build the BBC application stage and disk image targets through the normal
workspace build orchestration:

```sh
cd ../..
./scripts/build.sh confnio-bbc-disk
```

The BBC build is especially important: it verifies that the application still
fits below the configured BBC HIMEM address and that the 6502 assembly links
with the cc65-generated C objects.

Build the shortened-screen BBC splash proof and its standalone DFS image:

```sh
make splash-bbc-disk
```

## Beebium integration tests

### Prerequisites

Install or build the following before running the tests:

* Beebium, normally at `$HOME/dev/bbc/beebium`;
* `uv` and Python 3.12 (the runner can create the test environment with `uv`);
* the `fujinet-nio`, `fujinet-nio-lib`, and `fn-rom` repositories in the
  workspace layout above;
* a built fn-rom image, normally `repos/fn-rom/build/fujinet.rom`.

The test runner builds the required config-nio disk, BBC boot disk, and
fujinet-nio PTY binary automatically:

```sh
./integration-tests/beebium/run_pytest.sh -q
```

Run one test by its node id:

```sh
./integration-tests/beebium/run_pytest.sh \
  test_config_nio_bbc_real.py::test_config_nio_bbc_shows_fboot_runtime_mount -q
```

Run a focused group of tests using pytest's name filter:

```sh
./integration-tests/beebium/run_pytest.sh \
  test_config_nio_bbc_real.py -k 'slot or mount' -q
```

The runner creates screen evidence under `test-evidence/beebium-<timestamp>/`
unless evidence is disabled. To disable screenshots:

```sh
NIO_CONFIG_BEEBIUM_NO_EVIDENCE=1 \
  ./integration-tests/beebium/run_pytest.sh \
  test_config_nio_bbc_real.py::test_config_nio_bbc_shows_fboot_runtime_mount -q
```

If the binaries or repositories are in non-standard locations, override the
runner's defaults:

```sh
BEEBIUM_HOME=/path/to/beebium \
FUJINET_NIO_HOME=/path/to/fujinet-nio \
FUJINET_NIO_LIB=/path/to/fujinet-nio-lib \
FN_ROM_HOME=/path/to/fn-rom \
FN_ROM=/path/to/fujinet.rom \
./integration-tests/beebium/run_pytest.sh -q
```

The Beebium tests use an isolated fujinet-nio data directory for each test.
They do not modify a user's normal FujiNet configuration. A test can be
skipped when the requested fn-rom, Beebium assets, or PTY binary is missing;
pytest reports the missing prerequisite in that case.

## Useful diagnostics

Run pytest with output and stop on the first failure:

```sh
./integration-tests/beebium/run_pytest.sh -x -vv -s
```

The test runner's build output is written by the workspace build scripts under
`../../build/logs/`. Screen captures and the final screen state are kept in the
`test-evidence/` directory for failed and passed tests.

If the runner stops while building `fujinet-nio`, run the target build directly
to separate a nio-config failure from a fujinet-nio dependency/build failure:

```sh
cd ../fujinet-nio
cmake --build build/fujibus-pty-debug
```

## Amiga tests

Host tests (needs only gcc and the `fujinet-nio-lib` headers):

```sh
make test-amiga-host
make test-amiga-host ONLY=ctl_drives
```

Target builds (needs `m68k-amigaos-gcc` and `make -C ../fujinet-nio-lib amiga`):

```sh
make amiga AMIGA_PROFILE=wb32
make amiga AMIGA_PROFILE=wb13
```

The guest acceptance scripts in `tests/amiga/guest/` are run by the workspace
Amiberry suite (`integration-tests/amiberry/test_config_nio.py`); see
[docs/amiga-config.md](docs/amiga-config.md).

## Atari UI tests

Headless AltirraSDL (no window opens), in three layers under `tests/atari/`.
All need the workspace environment and the standard XEX built. The boot suite
also requires the banked build; emulator-dependent tests skip if the emulator is
unavailable:

```sh
source ../../scripts/env.sh
make TARGET=atari FUJINET_NIO_LIB=../fujinet-nio-lib
make atari-banked FUJINET_NIO_LIB=../fujinet-nio-lib
```

**Fast** (about 4 minutes; the embedded FujiNet-NIO device in AltirraSDL, the
XEX loaded directly, assertions on the app's screen memory and its row colours):

```sh
python3 -m unittest discover -s tests/atari -v
```

* `test_ui_smoke.py`, `test_ui_features.py`: tabs, hosts add/edit/delete, browsing,
  drive assignment/eject/write protect, names longer than a listing page, the name
  animation, the size/date line, directories of 100 entries, a 192 character URI,
  joystick and console keys, and state surviving a restart. A cycle-stepped
  Browse transition checks that the previous page stays monochrome until the
  new page is presented, that a failed refresh restores colours and entries,
  and that keyboard input does not write through a stale OS cursor pointer.
* `test_compose.py`: uses `soft65c02_unit` and the shared cc65-clib harness to
  compare the Atari 6502 URI composer against its C reference, including buffer
  limits and the 196-character boundary. It skips if the emulator or cc65 is missing.
  The script selects each host/path/leaf combination explicitly, primes result
  bytes to `0xff`, and checks the reported inputs, all 64 capacity checks, and
  success written only on completion. The final case checks the long boundary.
* `test_screenshots.py`: pixel comparison of the Hosts, Browse, assign dialog, Drives
  and host-prompt screens with `tests/atari/golden/*.png`, so offset or overlapping
  text and wrong colours fail. When the look changes on purpose, review the new
  picture and accept it with `UPDATE_GOLDEN=1 python3 -m unittest test_screenshots`
  (run from `tests/atari`). A failure saves the actual image in `golden/_actual/`.

**Boot** (slow, about 4 minutes; the real firmware over NetSIO, PicoBoot loading
the default boot disk) only with `NIO_ATARI_BOOT=1`:

```sh
cd tests/atari && NIO_ATARI_BOOT=1 python3 -m unittest test_boot -v
```

It checks that the app boots with BASIC present and absent and turns BASIC off
(PORTB, BASICF, MEMTOP), that RESET restarts it, and that OPTION boots an assigned
boot disk and an assigned XEX (the firmware's PicoBoot wrapper). It needs `dir2atr`,
`repos/fujinet-nio/build/atari-fujibus-netsio-debug` built from the current
firmware, and `proctest.xex` from `repos/nio-core-apps`.
It also checks the monochrome mount screen and that a failed mount restores the
normal tab and row-colour interrupts. The production selector is tested with
64 KB, 128 KB and 1088 KB memory models. Long-name cases verify traffic-level
reuse, packed bank-cache eviction, full 220-byte listing names, and drive assignment
across a switch through Setup. The simulated Wi-Fi test joins a network, rejects a
second one, checks rollback and persistence, then retries an open network. Both
Atari builds must exist for these cases.

Not covered: the display-interrupt/SIO failure that only showed on real hardware
(the emulator's serial timing is too forgiving); a hardware run is still needed
after changes to the screen code or the SIO path.

`scripts/atari_shot.py` and `scripts/atari_shot_netsio.py` run the same harness
from the command line with a key script and screenshots (see `docs/atari-config.md`).
