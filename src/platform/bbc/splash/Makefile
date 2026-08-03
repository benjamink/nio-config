SHELL := /usr/bin/env bash
.DEFAULT_GOAL := all

SPLASH_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(SPLASH_DIR)/../../../..)
BUILD_DIR ?= $(REPO_ROOT)/build/bbc/splash
OBJ_DIR := $(BUILD_DIR)/obj
STAGE_DIR := $(BUILD_DIR)/stage

CA65 ?= ca65
LD65 ?= ld65
PYTHON ?= python3
SCREEN_PYTHON ?= uv run --with Pillow python3
CREATE_SSD ?= $(REPO_ROOT)/../fujinet-nio-lib/scripts/create_ssd.py
SCREEN_INPUT ?= $(REPO_ROOT)/images/config-nio.png

PROGRAM_NAME ?= SPLASH
PROGRAM := $(BUILD_DIR)/$(PROGRAM_NAME)
OBJECT := $(OBJ_DIR)/$(PROGRAM_NAME).o
SCREEN := $(BUILD_DIR)/SCREEN
SSD := $(BUILD_DIR)/splash.ssd

.PHONY: all check clean disk stage config

all: $(PROGRAM) $(SCREEN) check

$(OBJECT): $(SPLASH_DIR)/main.s
	@mkdir -p "$(dir $@)"
	$(CA65) --cpu 6502 \
		$(if $(CONFIG_CHAIN),-D CONFIG_CHAIN=1,) \
		$(if $(CONFIG_MASTER),-D CONFIG_MASTER=1,) \
		--listing "$(@:.o=.lst)" -o "$@" "$<"

$(PROGRAM): $(OBJECT) $(SPLASH_DIR)/splash.cfg
	@mkdir -p "$(dir $@)"
	$(LD65) -C "$(SPLASH_DIR)/splash.cfg" -m "$(BUILD_DIR)/splash.map" -o "$@" "$<"

$(SCREEN): $(SPLASH_DIR)/screen_gen.py $(SCREEN_INPUT)
	$(SCREEN_PYTHON) "$<" $(if $(SCREEN_INPUT),--input "$(SCREEN_INPUT)",) --output "$@"

check: $(PROGRAM) $(SCREEN)
	@test "$$(wc -c < "$(SCREEN)")" -eq 3840
	@test "$$(wc -c < "$(PROGRAM)")" -le 1999

stage: all
	@mkdir -p "$(STAGE_DIR)"
	cp "$(PROGRAM)" "$(STAGE_DIR)/SPLASH"
	cp "$(SPLASH_DIR)/splash.inf" "$(STAGE_DIR)/SPLASH.inf"
	cp "$(SCREEN)" "$(STAGE_DIR)/SCREEN"
	cp "$(SPLASH_DIR)/screen.inf" "$(STAGE_DIR)/SCREEN.inf"

disk: stage
	$(PYTHON) "$(CREATE_SSD)" -i "$(STAGE_DIR)" -o "$(SSD)" -t SPLASH
	@echo "Built $(SSD)"

clean:
	rm -rf "$(BUILD_DIR)"

config:
	$(MAKE) clean all PROGRAM_NAME=CONFIG CONFIG_CHAIN=1 CONFIG_MASTER=$(CONFIG_MASTER)
