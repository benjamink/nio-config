# TRS-80 Model 4/4P config-nio, built with z88dk (sccz80) into raw binaries loaded at 3000h.
# Usage: make -C trs80            (needs z88dk; ZCCCFG defaults to /usr/share/z88dk/lib/config)
# The boot repo turns build/<name>.bin into an LS-DOS /CMD (tools/zcc_cmd.py).
ZCCCFG ?= /usr/share/z88dk/lib/config
export ZCCCFG
SRC     = ../src/platform/trs80
ZCC     = zcc +z80 -clib=classic -compiler=sccz80 -I$(SRC)
ASM     = z88dk-z80asm -b -mz80 -m -r0x3000 -L$(ZCCCFG)/../clibs/z80 -L$(ZCCCFG)/../clibs -lz80_clib -lndos -lz80_crt0
BUILD   = build
VPATH   = $(SRC)

# Objects every UI program links: console (C + asm SVC wrappers) and widgets.
COMMON  = $(BUILD)/trs80_console.o $(BUILD)/ui_widgets.o $(SRC)/trs80_console_asm.asm

PROGS   = hello keyprobe uidemo
all: $(PROGS:%=$(BUILD)/%.bin)

.SECONDARY:
$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(ZCC) -c -o $@ $<
$(BUILD)/hello.bin $(BUILD)/keyprobe.bin: $(BUILD)/%.bin: crt0.asm $(BUILD)/%.o
	$(ASM) -o$@ crt0.asm $(BUILD)/$*.o
$(BUILD)/uidemo.bin: crt0.asm $(BUILD)/uidemo.o $(COMMON)
	$(ASM) -o$@ crt0.asm $(BUILD)/uidemo.o $(COMMON)
clean:
	rm -rf $(BUILD)
.PHONY: all clean
