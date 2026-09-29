# Running nio-config tests

The repository currently has three practical test layers:

* target builds, which catch C/ASM, linker, and memory-limit regressions;
* BBC Beebium integration tests, which exercise `CONFNIO` through the BBC UI,
  fn-rom, and a real fujinet-nio PTY process;
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
source ~/.local/bin/add_watcom.sh
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
