SHELL := /usr/bin/env bash
.DEFAULT_GOAL := all

TARGET ?= msdos
FUJINET_NIO_LIB ?= ../fujinet-nio-lib
# cc65 checkout; BBC assembly includes its libsrc/bbc headers (oslib/os.inc).
CC65_HOME ?= ../cc65

include makefiles/targets.mk

FNSVC_LIST_MAX_PAYLOAD ?= 420
ifneq ($(filter $(TARGET),bbc master),)
FNSVC_LIST_MAX_PAYLOAD := 250
endif

SRC_DIR := src
APP_INCLUDE_DIR := include/common
CONFIG_NIO_INCLUDE_DIR := include
PLATFORM_INCLUDE_DIR := include/platform/$(PLATFORM)
TARGET_INCLUDE_DIR := include/target/$(TARGET)
NIO_INCLUDE_DIR := $(FUJINET_NIO_LIB)/include
BUILD_DIR ?= build
TARGET_BUILD_DIR ?= $(BUILD_DIR)/$(TARGET)

# Do not recreate the obsolete unprofiled Amiga output.  The repository
# Makefile supplies a named Workbench profile for public Amiga builds.
ifeq ($(TARGET),amiga)
ifeq ($(TARGET_BUILD_DIR),$(BUILD_DIR)/amiga)
$(error Amiga builds require TARGET_BUILD_DIR=build/amiga/wb13, wb31, or wb32; use `make amiga`)
endif
endif
OBJ_DIR := $(TARGET_BUILD_DIR)/obj
BIN_DIR := $(TARGET_BUILD_DIR)/bin
DISK_DIR := $(TARGET_BUILD_DIR)/disk

CONFIG_NIO_PROGRAMS := config-nio
SUPPORT_PROGRAMS_bbc := keycode
SUPPORT_PROGRAMS_master := keycode
SUPPORT_PROGRAMS := $(SUPPORT_PROGRAMS_$(TARGET))
PROGRAMS := $(CONFIG_NIO_PROGRAMS) $(SUPPORT_PROGRAMS)
PROGRAM_BINS := $(PROGRAMS:%=$(BIN_DIR)/%$(PROGRAM_EXT))

SUPPORT_OBJS := $(SUPPORT_PROGRAMS:%=$(OBJ_DIR)/$(SRC_DIR)/support/%.o)

ifeq ($(COMPILER_FAMILY),wcc)
include makefiles/compiler-wcc.mk
else ifeq ($(COMPILER_FAMILY),cc65)
include makefiles/compiler-cc65.mk
else ifeq ($(COMPILER_FAMILY),gcc)
include makefiles/compiler-gcc.mk
else ifeq ($(COMPILER_FAMILY),amigagcc)
include makefiles/compiler-amigagcc.mk
else
$(error Unknown compiler family '$(COMPILER_FAMILY)' for TARGET=$(TARGET))
endif

rwildcard=$(wildcard $(1)$(2)) $(foreach d,$(wildcard $(1)*),$(call rwildcard,$d/,$(2)))

CONFIG_NIO_EXTRA_SRC_DIRS_msdos := $(SRC_DIR)/platform/portable
CONFIG_NIO_EXTRA_SRC_DIRS_atari := $(SRC_DIR)/platform/portable
CONFIG_NIO_EXTRA_SRC_DIRS_linux := $(SRC_DIR)/platform/portable
CONFIG_NIO_EXTRA_SRC_DIRS := $(CONFIG_NIO_EXTRA_SRC_DIRS_$(TARGET))

CONFIG_NIO_SRC_DIRS := \
	$(SRC_DIR)/common \
	$(CONFIG_NIO_EXTRA_SRC_DIRS) \
	$(SRC_DIR)/platform/$(PLATFORM) \
	$(SRC_DIR)/target/$(TARGET)

CONFIG_NIO_SRCS := $(wildcard $(SRC_DIR)/*.c)
CONFIG_NIO_ASM_SRCS :=
$(foreach dir,$(CONFIG_NIO_SRC_DIRS),$(eval CONFIG_NIO_SRCS += $(call rwildcard,$(dir)/,*.c)))
$(foreach dir,$(CONFIG_NIO_SRC_DIRS),$(eval CONFIG_NIO_ASM_SRCS += $(call rwildcard,$(dir)/,*.s)))
CONFIG_NIO_SRCS := $(foreach src,$(CONFIG_NIO_SRCS),$(if $(findstring /support/,$(src)),,$(src)))
CONFIG_NIO_ASM_SRCS := $(foreach src,$(CONFIG_NIO_ASM_SRCS),$(if $(findstring /support/,$(src)),,$(src)))
# The splash loader is a separate executable with its own address map and
# Makefile; never link it into the main config-nio application.
CONFIG_NIO_SRCS := $(foreach src,$(CONFIG_NIO_SRCS),$(if $(findstring /splash/,$(src)),,$(src)))
CONFIG_NIO_ASM_SRCS := $(foreach src,$(CONFIG_NIO_ASM_SRCS),$(if $(findstring /splash/,$(src)),,$(src)))
# Amiga has its own main() and Intuition front end.  It links only the
# portable state/store/table/service layers, not the text UI loops, plus the
# platform-neutral app-store fnctl used by Linux.
CONFIG_NIO_EXTRA_SRCS_amiga := \
	$(SRC_DIR)/platform/portable/config_nio_state.c \
	$(SRC_DIR)/platform/portable/config_nio_store.c \
	$(SRC_DIR)/platform/portable/config_nio_tables.c \
	$(SRC_DIR)/platform/portable/fnsvc.c \
	$(SRC_DIR)/platform/linux/fnctl.c
CONFIG_NIO_SRCS += $(CONFIG_NIO_EXTRA_SRCS_$(TARGET))
ifeq ($(TARGET),amiga)
CONFIG_NIO_SRCS := $(filter-out $(SRC_DIR)/main.c,$(CONFIG_NIO_SRCS))
endif
# Both BBC targets use the smaller assembly mapping-mount wrapper.  Master
# shares the same BBC state API; its XRAM table selection does not change this
# wrapper's interface or storage requirements.
ifneq ($(filter $(TARGET),bbc master),)
CONFIG_NIO_SRCS := $(filter-out $(SRC_DIR)/platform/bbc/config_nio_state_bbc.c,$(CONFIG_NIO_SRCS))
CONFIG_NIO_ASM_SRCS := $(filter-out $(SRC_DIR)/platform/bbc/bbc_oscli.s,$(CONFIG_NIO_ASM_SRCS))
endif
CONFIG_NIO_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CONFIG_NIO_SRCS))
CONFIG_NIO_ASM_OBJS := $(patsubst %.s,$(OBJ_DIR)/%.o,$(CONFIG_NIO_ASM_SRCS))
DEPENDS := $(SUPPORT_OBJS:.o=.d) $(CONFIG_NIO_OBJS:.o=.d)

ifneq ($(filter $(TARGET),bbc master),)
CONFIG_NIO_BBC_TEMPLATE_INPUTS := \
	bbc/assets/config-nio-templates/CNHOSTS \
	bbc/assets/config-nio-templates/CNBROW \
	bbc/assets/config-nio-templates/CNSLOTS \
	bbc/config_nio_layout.json \
	bbc/scripts/generate_config_nio_templates.py
CONFIG_NIO_BBC_GENERATED := \
	$(SRC_DIR)/platform/$(PLATFORM)/config_nio_template_data.s \
	$(SRC_DIR)/platform/$(PLATFORM)/config_nio_layout.h \
	$(SRC_DIR)/platform/$(PLATFORM)/config_nio_layout.inc

$(CONFIG_NIO_BBC_GENERATED): $(CONFIG_NIO_BBC_TEMPLATE_INPUTS)
	python3 bbc/scripts/generate_config_nio_templates.py

$(OBJ_DIR)/$(SRC_DIR)/platform/$(PLATFORM)/config_nio_template_data.o: $(CONFIG_NIO_BBC_GENERATED)
$(OBJ_DIR)/$(SRC_DIR)/platform/$(PLATFORM)/config_nio_template_decompress.o: $(CONFIG_NIO_BBC_GENERATED)
$(OBJ_DIR)/$(SRC_DIR)/platform/$(PLATFORM)/config_nio_ui.o: $(SRC_DIR)/platform/$(PLATFORM)/config_nio_layout.h
endif

ifeq ($(TARGET),msdos)
PDCURSES_DIR ?= ../PDCurses
PDCURSES_MSDOS_LIB ?= ../../build/pdcurses/msdos-small/pdcurses.lib
CONFIG_NIO_LIBS := $(PDCURSES_MSDOS_LIB)
CONFIG_NIO_DEPS := $(PDCURSES_MSDOS_LIB)
CFLAGS += -i=$(PDCURSES_DIR)
endif

ifneq ($(filter $(TARGET),bbc master),)
include makefiles/config-nio-bbc.mk
endif

.PHONY: all clean $(PROGRAMS)
.SECONDARY: $(SUPPORT_OBJS) $(CONFIG_NIO_OBJS) $(CONFIG_NIO_ASM_OBJS)

all: $(PROGRAM_BINS)

$(PROGRAMS): %: $(BIN_DIR)/%$(PROGRAM_EXT)

-include $(DEPENDS)

$(NIO_LIB_FILE):
	$(MAKE) -C $(FUJINET_NIO_LIB) $(NIO_LIB_TARGET)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(call compile_c)

$(OBJ_DIR)/%.o: %.s | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	ca65 -t $(TOOLCHAIN_TARGET) $(ASMFLAGS) --listing $(@:.o=.lst) -I $(CC65_HOME)/libsrc/bbc -o $@ $<

$(BIN_DIR)/keycode$(PROGRAM_EXT): $(OBJ_DIR)/$(SRC_DIR)/support/keycode.o | $(BIN_DIR)
	$(call link_program)

$(BIN_DIR)/config-nio$(PROGRAM_EXT): $(CONFIG_NIO_OBJS) $(CONFIG_NIO_ASM_OBJS) $(CONFIG_NIO_DEPS) $(NIO_LIB_FILE) | $(BIN_DIR)
	$(call link_program)

ifneq ($(filter $(TARGET),bbc master),)
BBC_CONFIG_NIO_START_ADDRESS_bbc ?= 0x1900
BBC_CONFIG_NIO_START_ADDRESS_master ?= 0x0E00
BBC_CONFIG_NIO_HIMEM_bbc ?= 0x7C00
BBC_CONFIG_NIO_HIMEM_master ?= 0x8000
BBC_CONFIG_NIO_SHADOW_MODE_bbc ?= 0
BBC_CONFIG_NIO_SHADOW_MODE_master ?= 1
BBC_CONFIG_NIO_XRAM_TABLES_bbc ?= 0
BBC_CONFIG_NIO_XRAM_TABLES_master ?= 1
BBC_CONFIG_NIO_START_ADDRESS ?= $(BBC_CONFIG_NIO_START_ADDRESS_$(TARGET))
BBC_CONFIG_NIO_HIMEM ?= $(BBC_CONFIG_NIO_HIMEM_$(TARGET))
BBC_CONFIG_NIO_SHADOW_MODE ?= $(BBC_CONFIG_NIO_SHADOW_MODE_$(TARGET))
BBC_CONFIG_NIO_XRAM_TABLES ?= $(BBC_CONFIG_NIO_XRAM_TABLES_$(TARGET))
$(BIN_DIR)/config-nio$(PROGRAM_EXT): CFLAGS += -DCONFIG_NIO_EXTERNAL_TABLE_STATE
ifeq ($(BBC_CONFIG_NIO_SHADOW_MODE),1)
$(BIN_DIR)/config-nio$(PROGRAM_EXT): CFLAGS += -DCONFIG_NIO_BBC_SHADOW_MODE
endif
ifeq ($(BBC_CONFIG_NIO_XRAM_TABLES),1)
$(BIN_DIR)/config-nio$(PROGRAM_EXT): CFLAGS += -DCONFIG_NIO_BBC_XRAM_TABLES
$(CONFIG_NIO_ASM_OBJS): ASMFLAGS += -DCONFIG_NIO_BBC_XRAM_TABLES
endif
$(BIN_DIR)/config-nio$(PROGRAM_EXT): LDFLAGS := -t $(TOOLCHAIN_TARGET) --start-addr $(BBC_CONFIG_NIO_START_ADDRESS) -Wl -D,__HIMEM__=$(BBC_CONFIG_NIO_HIMEM)
endif

$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

$(DISK_DIR):
	mkdir -p $@

clean:
	rm -rf $(TARGET_BUILD_DIR)
