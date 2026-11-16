# TRS-80 Model 4/4P config-nio, built with z88dk (sccz80) into raw binaries loaded at 3000h.
# Usage: make -C trs80            (needs z88dk; ZCCCFG defaults to /usr/share/z88dk/lib/config)
# build/<name>.bin is code+data only (bss is cleared by crt0, see trimbss.sh); the boot repo turns it into an
# LS-DOS /CMD (tools/zcc_cmd.py), adding the nioclient segment at D000h for programs that talk to the FujiNet.
ZCCCFG ?= /usr/share/z88dk/lib/config
export ZCCCFG
SRC     = ../src/platform/trs80
FUJINET_NIO_LIB ?= ../../fujinet-nio-lib
# State limits sized for the 4P's memory (see docs/trs80-config.md): fewer/shorter entries than MS-DOS.
LIMITS  = -DCONFIG_NIO_URI_MAX=127 -DCONFIG_NIO_MAX_ENTRIES=12 -DCONFIG_NIO_NAME_MAX=47 -DCONFIG_NIO_TEXT_MAX=1100
ZCC     = zcc +z80 -clib=classic -compiler=sccz80 $(LIMITS) -I$(SRC) -I../include/common -I../include -I$(FUJINET_NIO_LIB)/include
ASM     = z88dk-z80asm -b -mz80 -m -r0x3000 -L$(ZCCCFG)/../clibs/z80 -L$(ZCCCFG)/../clibs -lz80_clib -lndos -lz80_crt0
BUILD   = build
VPATH   = $(SRC):../src/platform/portable:$(FUJINET_NIO_LIB)/src/common

# Objects every UI program links: console (C + asm SVC wrappers) and widgets.
COMMON  = $(BUILD)/trs80_console.o $(BUILD)/ui_widgets.o $(SRC)/trs80_console_asm.asm
# The shared service code (fnsvc) over fnctl_trs80.c / fn_raw_trs80.c / nio_glue.asm.
NIO     = $(BUILD)/fnsvc.o $(BUILD)/fnctl_trs80.o $(BUILD)/fn_raw_trs80.o $(SRC)/nio_glue.asm
# State, tables and store from the portable layer, over the library's app-store and slot-catalogue calls.
LIBSRC  = fn_appstore_common fn_appstore_default fn_appstore_read fn_appstore_write fn_appstore_delete \
          fn_slot_catalog_common fn_slot_catalog_default fn_slot_catalog_get fn_slot_catalog_put \
          fn_slot_catalog_delete fn_slot_catalog_validate_default fn_slot_catalog_range fn_slot_catalog_next_entry
STORE   = $(BUILD)/config_nio_store.o $(BUILD)/config_nio_state.o $(BUILD)/config_nio_tables.o \
          $(LIBSRC:%=$(BUILD)/%.o)

PROGS   = hello keyprobe uidemo nio_selftest store_selftest
all: $(PROGS:%=$(BUILD)/%.bin)

.SECONDARY:
$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(ZCC) -c -o $@ $<

# link <name> <objects...>
name = $(basename $(notdir $@))
link = $(ASM) -o$@ crt0.asm $(BUILD)/$(name).o $(1) && ./trimbss.sh $(name)
$(BUILD)/hello.bin $(BUILD)/keyprobe.bin: $(BUILD)/%.bin: crt0.asm $(BUILD)/%.o
	$(call link,)
$(BUILD)/uidemo.bin: crt0.asm $(BUILD)/uidemo.o $(COMMON)
	$(call link,$(COMMON))
$(BUILD)/nio_selftest.bin: crt0.asm $(BUILD)/nio_selftest.o $(NIO)
	$(call link,$(NIO))
$(BUILD)/store_selftest.bin: crt0.asm $(BUILD)/store_selftest.o $(STORE) $(NIO)
	$(call link,$(STORE) $(NIO))
clean:
	rm -rf $(BUILD)
.PHONY: all clean
