# nio-config

The configuration application for FujiNet-NIO platforms.

This repository owns `config-nio` and its generated BBC teletext template
assets. It also carries the BBC `keycode` support tool while the BBC config
disk/stage flow needs it.

See [RUNNING_TESTS.md](RUNNING_TESTS.md) for copy-and-run build and Beebium
integration-test commands.

Supported default targets are `msdos`, `bbc`, `linux`, and `atari`.

```sh
make TARGET=msdos FUJINET_NIO_LIB=../fujinet-nio-lib
make TARGET=bbc FUJINET_NIO_LIB=../fujinet-nio-lib
make TARGET=linux FUJINET_NIO_LIB=../fujinet-nio-lib
make TARGET=atari FUJINET_NIO_LIB=../fujinet-nio-lib
```

## Atari

The Atari build is the machine's launcher: a 40x24 full-screen application
(Hosts, Browse, Drives, Setup) that is the default boot disk of the FujiNet.
Choose images, assign them to D1-D8 and press OPTION to boot with them
mounted. See [docs/atari-config.md](docs/atari-config.md) for the design,
the memory layout, and how to run and test it in the emulator.

BBC stage targets:

```sh
make -f makefiles/build.mk TARGET=bbc config-nio-bbc-stage
make -f makefiles/build.mk TARGET=bbc config-nio-master-stage
```

The template source files live in `bbc/assets/config-nio-templates/`. Running a
BBC build regenerates the compressed table source from those 1000-byte template
files and `bbc/config_nio_layout.json`.

## Amiga

The Amiga Workbench (1.3 and later) version is built separately for each
Workbench profile and is not part of `all-targets`:

```sh
make amiga                         # builds wb13, wb31, and wb32
make amiga AMIGA_PROFILE=wb13      # one explicit profile
make test-amiga-host              # host-side logic tests (gcc only)
```

See [docs/amiga-config.md](docs/amiga-config.md).
