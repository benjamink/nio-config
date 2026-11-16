# TRS-80 Model 4/4P config-nio, built with z88dk (sccz80) into raw binaries loaded at 3000h.
# Usage: make -C trs80            (needs z88dk; ZCCCFG defaults to /usr/share/z88dk/lib/config)
# The boot repo turns build/<name>.bin into an LS-DOS /CMD (tools/zcc_cmd.py).
ZCCCFG ?= /usr/share/z88dk/lib/config
export ZCCCFG
SRC     = ../src/platform/trs80
FUJINET_NIO_LIB ?= ../../fujinet-nio-lib
ZCC     = zcc +z80 -clib=classic -compiler=sccz80 -I$(SRC) -I../include/common -I../include -I$(FUJINET_NIO_LIB)/include
ASM     = z88dk-z80asm -b -mz80 -m -r0x3000 -L$(ZCCCFG)/../clibs/z80 -L$(ZCCCFG)/../clibs -lz80_clib -lndos -lz80_crt0
BUILD   = build
VPATH   = $(SRC):../src/platform/portable

# Objects every UI program links: console (C + asm SVC wrappers) and widgets.
COMMON  = $(BUILD)/trs80_console.o $(BUILD)/ui_widgets.o $(SRC)/trs80_console_asm.asm

PROGS   = hello keyprobe uidemo nio_selftest
all: $(PROGS:%=$(BUILD)/%.bin)

.SECONDARY:
$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(ZCC) -c -o $@ $<
$(BUILD)/hello.bin $(BUILD)/keyprobe.bin: $(BUILD)/%.bin: crt0.asm $(BUILD)/%.o
	$(ASM) -o$@ crt0.asm $(BUILD)/$*.o
$(BUILD)/uidemo.bin: crt0.asm $(BUILD)/uidemo.o $(COMMON)
	$(ASM) -o$@ crt0.asm $(BUILD)/uidemo.o $(COMMON)
# The shared service code (fnsvc) over fnctl_trs80.c / fn_raw_trs80.c / nio_glue.asm.
NIO     = $(BUILD)/fnsvc.o $(BUILD)/fnctl_trs80.o $(BUILD)/fn_raw_trs80.o $(SRC)/nio_glue.asm
$(BUILD)/nio_selftest.bin: crt0.asm $(BUILD)/nio_selftest.o $(NIO)
	$(ASM) -o$@ crt0.asm $(BUILD)/nio_selftest.o $(NIO)
clean:
	rm -rf $(BUILD)
.PHONY: all clean
