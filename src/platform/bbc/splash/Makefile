SHELL := /usr/bin/env bash
.DEFAULT_GOAL := all

SPLASH_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(SPLASH_DIR)/../../../..)
WORKSPACE_ROOT := $(abspath $(REPO_ROOT)/../..)
BUILD_DIR ?= $(REPO_ROOT)/build/bbc/splash
OBJ_DIR := $(BUILD_DIR)/obj
STAGE_DIR := $(BUILD_DIR)/stage

CA65 ?= ca65
LD65 ?= ld65
PYTHON ?= python3
CREATE_SSD ?= $(REPO_ROOT)/scripts/create_ssd.py

# Optional workspace tools — only needed for `regen-screen`.
BBC_IMAGE ?= $(WORKSPACE_ROOT)/scripts/bbc-image
COMPRESS ?= $(WORKSPACE_ROOT)/scripts/compress

SCREEN_INPUT ?= $(REPO_ROOT)/images/config-nio-160x96x4.png
SCREEN_PALETTE ?= black,blue,yellow,white

# Checked-in assets used by the default build (no workspace tools required).
SCREEN_ASSET := $(SPLASH_DIR)/SCREEN
SCREENZ_ASSET := $(SPLASH_DIR)/SCREENZ
PALETTE_INC := $(SPLASH_DIR)/palette.inc

PROGRAM_NAME ?= SPLASH
PROGRAM := $(BUILD_DIR)/$(PROGRAM_NAME)
OBJECT := $(OBJ_DIR)/$(PROGRAM_NAME).o
PREVIEW := $(BUILD_DIR)/config-nio-preview.png
SSD := $(BUILD_DIR)/splash.ssd

.PHONY: all check clean disk stage config regen-screen

all: $(PROGRAM) check

$(OBJECT): $(SPLASH_DIR)/main.s $(PALETTE_INC)
	@mkdir -p "$(dir $@)"
	$(CA65) --cpu 6502 \
		$(if $(CONFIG_CHAIN),-D CONFIG_CHAIN=1,) \
		$(if $(CONFIG_MASTER),-D CONFIG_MASTER=1,) \
		--listing "$(@:.o=.lst)" -o "$@" "$<"

$(PROGRAM): $(OBJECT) $(SPLASH_DIR)/splash.cfg
	@mkdir -p "$(dir $@)"
	$(LD65) -C "$(SPLASH_DIR)/splash.cfg" -m "$(BUILD_DIR)/splash.map" -o "$@" "$<"

check: $(PROGRAM)
	@test -f "$(SCREENZ_ASSET)"
	@test "$$(wc -c < "$(SCREENZ_ASSET)")" -gt 0
	@test "$$(wc -c < "$(SCREENZ_ASSET)")" -lt 3840
	@test "$$(wc -c < "$(PROGRAM)")" -le 1999

stage: all
	@mkdir -p "$(STAGE_DIR)"
	cp "$(PROGRAM)" "$(STAGE_DIR)/SPLASH"
	cp "$(SPLASH_DIR)/splash.inf" "$(STAGE_DIR)/SPLASH.inf"
	cp "$(SCREENZ_ASSET)" "$(STAGE_DIR)/SCREENZ"
	cp "$(SPLASH_DIR)/screenz.inf" "$(STAGE_DIR)/SCREENZ.inf"

disk: stage
	$(PYTHON) "$(CREATE_SSD)" -i "$(STAGE_DIR)" -o "$(SSD)" -t SPLASH
	@echo "Built $(SSD)"

# Rebuild checked-in SCREEN / SCREENZ / palette.inc from the source PNG.
# Requires the workspace scripts (bbc-image, compress). Normal builds skip this.
regen-screen:
	@test -x "$(BBC_IMAGE)" || { echo "regen-screen needs $(BBC_IMAGE)" >&2; exit 1; }
	@test -x "$(COMPRESS)" || { echo "regen-screen needs $(COMPRESS)" >&2; exit 1; }
	@mkdir -p "$(BUILD_DIR)"
	$(BBC_IMAGE) "$(SCREEN_INPUT)" "$(BUILD_DIR)/SCREEN" \
		--mode 5 \
		--palette "$(SCREEN_PALETTE)" \
		--map-source-levels \
		--dither none \
		--preview "$(PREVIEW)" \
		--ula-palette-asm "$(PALETTE_INC)"
	$(COMPRESS) -i "$(BUILD_DIR)/SCREEN" -o "$(SCREENZ_ASSET)"
	cp "$(BUILD_DIR)/SCREEN" "$(SCREEN_ASSET)"
	@test "$$(wc -c < "$(SCREEN_ASSET)")" -eq 3840
	@echo "Updated $(SCREEN_ASSET), $(SCREENZ_ASSET), and $(PALETTE_INC)"

clean:
	rm -rf "$(BUILD_DIR)"

config:
	$(MAKE) clean all PROGRAM_NAME=CONFIG CONFIG_CHAIN=1 CONFIG_MASTER=$(CONFIG_MASTER)
