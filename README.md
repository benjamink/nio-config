# nio-config

The configuration application for FujiNet-NIO platforms.

This repository owns `config-nio` and its generated BBC teletext template
assets. It also carries the BBC `keycode` support tool while the BBC config
disk/stage flow needs it.

See [RUNNING_TESTS.md](RUNNING_TESTS.md) for copy-and-run build and Beebium
integration-test commands.

Supported default targets are `msdos`, `bbc`, and `linux`.

```sh
make TARGET=msdos FUJINET_NIO_LIB=../fujinet-nio-lib
make TARGET=bbc FUJINET_NIO_LIB=../fujinet-nio-lib
make TARGET=linux FUJINET_NIO_LIB=../fujinet-nio-lib
```

There is still Atari UI source in the tree, but the current Atari link exceeds
the existing cc65 Atari memory layout and is not part of `all-targets`.

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
make amiga AMIGA_PROFILE=wb32     # or wb31, wb13
make test-amiga-host              # host-side logic tests (gcc only)
```

See [docs/amiga-config.md](docs/amiga-config.md).
