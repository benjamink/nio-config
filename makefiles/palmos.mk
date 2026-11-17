# Native Palm-only UI; other platform source lists/flags remain unchanged.
FUJINET_NIO_LIB ?= ../fujinet-nio-lib
PALMOS_SDK_VERSION ?= 3.5
NIOLIB_BUILD ?= $(FUJINET_NIO_LIB)/build/palmos-shared-$(PALMOS_SDK_VERSION)
PALMOS_BUILD ?= build/palmos-usb
CC = m68k-palmos-gcc
FLAGS = -O2 -Wall -Werror -palmos$(PALMOS_SDK_VERSION) -I$(FUJINET_NIO_LIB)/include/palmos -I$(FUJINET_NIO_LIB)/include
ifeq ($(PALMOS_SDK_VERSION),3.1)
FLAGS += -DNIO_PALMOS_SDK31
endif
SOURCES = src/platform/palmos/config.c src/platform/palmos/browser.c
HEADERS = src/platform/palmos/palm_config.h src/platform/palmos/browser.h $(wildcard $(FUJINET_NIO_LIB)/include/palmos/*.h) $(wildcard $(FUJINET_NIO_LIB)/include/*.h)
# Explicit resources keep files left by older builds out of the PRC.
RESOURCES = $(addprefix $(PALMOS_BUILD)/,tFRM03e8.bin tFRM044c.bin tFRM04b0.bin \
 tFRM0514.bin tFRM0578.bin tFRM05dc.bin tFRM0640.bin Talt238c.bin Talt238d.bin Talt238e.bin Talt238f.bin tAIN03e8.bin)
.PHONY: all
all: $(PALMOS_BUILD)/NioConfig.prc
$(PALMOS_BUILD):
	mkdir -p $@
$(PALMOS_BUILD)/NioConfig: $(SOURCES) $(HEADERS) $(NIOLIB_BUILD)/libniolib-client.a | $(PALMOS_BUILD)
	$(CC) $(FLAGS) -Wl,-Map,$@.map -o $@ $(SOURCES) $(NIOLIB_BUILD)/libniolib-client.a
	m68k-palmos-nm -n $@ > $@.symbols
	m68k-palmos-objdump -d $@ > $@.dis
$(PALMOS_BUILD)/resources.stamp: src/platform/palmos/config.rcp | $(PALMOS_BUILD)
	pilrc -q $< $(PALMOS_BUILD)
	touch $@
$(PALMOS_BUILD)/NioConfig.prc: $(PALMOS_BUILD)/NioConfig $(PALMOS_BUILD)/resources.stamp makefiles/palmos.mk
	build-prc -c NiC1 -n "NIO Config" -o $@ $< $(RESOURCES)
