# TRS-80 Model 4/4P config-nio, built with z88dk (sccz80) into a raw binary loaded at 3000h.
# Usage: make -C trs80            (needs z88dk; ZCCCFG defaults to /usr/share/z88dk/lib/config)
# The boot repo turns build/<name>.bin into an LS-DOS /CMD (tools/zcc_cmd.py).
ZCCCFG ?= /usr/share/z88dk/lib/config
export ZCCCFG
ZCC     = zcc +z80 -clib=classic -compiler=sccz80
ASM     = z88dk-z80asm -b -mz80 -m -r0x3000
BUILD   = build

all: $(BUILD)/hello.bin
$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(ZCC) -c -o $@ $<
$(BUILD)/hello.bin: crt0.asm $(BUILD)/hello.o
	$(ASM) -o$@ crt0.asm $(BUILD)/hello.o
clean:
	rm -rf $(BUILD)
.PHONY: all clean
