CC := m68k-amigaos-gcc
# One binary for every Workbench: libnix (nix13) starts on Kickstart 1.3 and
# later, and the 2.0+ calls are guarded by runtime library-version checks.
AMIGA_CRT ?= nix13
AMIGA_COMPILE_CRT ?= nix20

CFLAGS += -Wall -Wextra -O2 -std=c99
CFLAGS += -mcpu=68000 -msoft-float
# Compile against the 2.0 NDK (nix20 headers): the 1.3 NDK that nix13 selects
# lacks the 2.0+ calls (SystemTags, workbench.library, DrawInfo) used when the
# OS has them.  Link with the nix13 runtime, which starts on Kickstart 1.3.
CFLAGS += -mcrt=$(AMIGA_COMPILE_CRT)
CFLAGS += -I$(APP_INCLUDE_DIR)
CFLAGS += -I$(CONFIG_NIO_INCLUDE_DIR)
CFLAGS += -I$(PLATFORM_INCLUDE_DIR)
CFLAGS += -I$(NIO_INCLUDE_DIR)
CFLAGS += -DFNSVC_LIST_MAX_PAYLOAD=$(FNSVC_LIST_MAX_PAYLOAD)
CFLAGS += -DCONFIG_NIO_MAX_ENTRIES=200 -DCONFIG_NIO_NAME_MAX=220
CFLAGS += -DCONFIG_NIO_MAX_HOSTS=64 -DCONFIG_NIO_TEXT_MAX=16384
# ListPages replies need the 1024-byte FujiBus packet the Amiga lib uses.
CFLAGS += -DFNCTL_MAX_DATA=1024
CFLAGS += -D__AMIGA__

LDFLAGS += -mcpu=68000 -msoft-float -mcrt=$(AMIGA_CRT)

define compile_c
	$(CC) $(CFLAGS) -MMD -MF $(@:.o=.d) -c -o $@ $<
endef

define link_program
	$(CC) -o $@ $^ $(LDFLAGS) $(EXTRA_PROGRAM_LDFLAGS) -lamiga
endef
