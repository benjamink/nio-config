# Host-side (gcc) tests for the Amiga config-nio logic.  Needs no Amiga
# toolchain and no FujiNet: tests/amiga/fake_nio.c stands in for the
# fujinet-nio library calls used by the portable store and the controller.
HOSTCC ?= cc
FUJINET_NIO_LIB ?= ../fujinet-nio-lib
BUILD_DIR ?= build
TEST_DIR := $(BUILD_DIR)/test-amiga-host
TEST_BIN := $(TEST_DIR)/run_tests

TEST_CFLAGS := -std=c99 -Wall -Wextra -O0 -g \
	-Iinclude -Iinclude/common -Iinclude/platform/amiga -Itests/amiga \
	-I$(FUJINET_NIO_LIB)/include \
	-DFNSVC_LIST_MAX_PAYLOAD=420

AMIGA_HOST_SRCS := \
	src/platform/amiga/amiga_drives.c \
	src/platform/amiga/amiga_list.c
PORTABLE_HOST_SRCS :=
TEST_SUPPORT_SRCS := tests/amiga/run_tests.c
TEST_SRCS := $(wildcard tests/amiga/test_*.c)

.PHONY: all
all: $(TEST_BIN)
	$(TEST_BIN) $(ONLY)

$(TEST_BIN): $(TEST_SUPPORT_SRCS) $(TEST_SRCS) $(AMIGA_HOST_SRCS) $(PORTABLE_HOST_SRCS) \
		$(wildcard tests/amiga/*.h tests/amiga/*.def include/platform/amiga/*.h include/*.h) \
		makefiles/test-amiga-host.mk | $(TEST_DIR)
	$(HOSTCC) $(TEST_CFLAGS) -o $@ $(filter %.c,$^)

$(TEST_DIR):
	mkdir -p $@
