# Native Palm-only UI; other platform source lists/flags remain unchanged.
FUJINET_NIO_LIB ?= ../fujinet-nio-lib
NIOLIB_BUILD ?= $(FUJINET_NIO_LIB)/build/palmos-shared-usb
PALMOS_BUILD ?= build/palmos-usb
CC = m68k-palmos-gcc
FLAGS = -O2 -Wall -Werror -palmos3.5 -I$(FUJINET_NIO_LIB)/include/palmos
.PHONY: all
all: $(PALMOS_BUILD)/NioConfig.prc
$(PALMOS_BUILD):
	mkdir -p $@
$(PALMOS_BUILD)/NioConfig: src/platform/palmos/config.c $(FUJINET_NIO_LIB)/include/palmos/NioLib.h $(NIOLIB_BUILD)/libniolib-client.a | $(PALMOS_BUILD)
	$(CC) $(FLAGS) -Wl,-Map,$@.map -o $@ $< $(NIOLIB_BUILD)/libniolib-client.a
	m68k-palmos-nm -n $@ > $@.symbols
	m68k-palmos-objdump -d $@ > $@.dis
$(PALMOS_BUILD)/resources.stamp: src/platform/palmos/config.rcp | $(PALMOS_BUILD)
	pilrc -q $< $(PALMOS_BUILD)
	touch $@
$(PALMOS_BUILD)/NioConfig.prc: $(PALMOS_BUILD)/NioConfig $(PALMOS_BUILD)/resources.stamp
	build-prc -c NiC1 -n "NIO Config" -o $@ $< $(PALMOS_BUILD)/tFRM03e8.bin $(PALMOS_BUILD)/tAIN03e8.bin
