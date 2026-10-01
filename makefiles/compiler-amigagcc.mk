CC := m68k-amigaos-gcc
AMIGA_CRT ?= clib2
AMIGA_WB13 ?= 0

CFLAGS += -Wall -Wextra -O2 -std=c99
CFLAGS += -mcpu=68000 -msoft-float
# Select the CRT's headers as well as its libraries (see nio-core-apps).
CFLAGS += -mcrt=$(AMIGA_CRT)
CFLAGS += -I$(APP_INCLUDE_DIR)
CFLAGS += -I$(CONFIG_NIO_INCLUDE_DIR)
CFLAGS += -I$(PLATFORM_INCLUDE_DIR)
CFLAGS += -I$(NIO_INCLUDE_DIR)
CFLAGS += -DFNSVC_LIST_MAX_PAYLOAD=$(FNSVC_LIST_MAX_PAYLOAD)
CFLAGS += -DCONFIG_NIO_MAX_ENTRIES=200
CFLAGS += -D__AMIGA__
ifeq ($(AMIGA_WB13),1)
CFLAGS += -D__KICK13__
endif

LDFLAGS += -mcpu=68000 -msoft-float -mcrt=$(AMIGA_CRT)

define compile_c
	$(CC) $(CFLAGS) -MMD -MF $(@:.o=.d) -c -o $@ $<
endef

define link_program
	$(CC) -o $@ $^ $(LDFLAGS) $(EXTRA_PROGRAM_LDFLAGS) -lamiga
endef
