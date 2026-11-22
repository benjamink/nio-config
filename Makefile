TARGETS := msdos bbc master linux atari c64
DEFAULT_TARGET := $(if $(TARGET),$(TARGET),all-targets)

.PHONY: atari-banked all all-targets clean disk confnio-bbc-disk confnio-master-disk \
	splash-bbc splash-bbc-disk test-amiga-host test-amiga-bootdisk amiga amiga-bootdisk \
	regen-amiga-icons $(TARGETS)

all: $(DEFAULT_TARGET)

$(TARGETS):
	$(MAKE) -f makefiles/build.mk TARGET=$@

all-targets: $(TARGETS)

atari-banked:
	$(MAKE) -f makefiles/build.mk TARGET=atari ATARI_BANKED=1 TARGET_BUILD_DIR=build/atari-banked
	@mkdir -p build/atari-banked/bin build/atari-banked/obj
	ca65 -t atari -o build/atari-banked/obj/loader.o src/atari-bootstrap/loader.s
	ld65 -C makefiles/atari-banked/loader.cfg -o build/atari-banked/bin/loader.xex build/atari-banked/obj/loader.o


disk:
	$(MAKE) -f makefiles/build.mk TARGET=$(if $(TARGET),$(TARGET),msdos) disk

config-nio-bbc-stage config-nio-master-stage:
	$(MAKE) -f makefiles/build.mk TARGET=$(if $(findstring master,$@),master,bbc) $@

confnio-bbc-disk:
	$(MAKE) -f makefiles/build.mk TARGET=bbc config-nio-bbc-stage

confnio-master-disk:
	$(MAKE) -f makefiles/build.mk TARGET=master config-nio-master-stage

splash-bbc:
	$(MAKE) -C src/platform/bbc/splash

splash-bbc-disk:
	$(MAKE) -C src/platform/bbc/splash disk

clean:
	rm -rf build

test-amiga-host:
	$(MAKE) -f makefiles/test-amiga-host.mk FUJINET_NIO_LIB=$(or $(FUJINET_NIO_LIB),../fujinet-nio-lib) ONLY=$(ONLY)

# Amiga is built on demand (not part of all-targets), always into a named
# Workbench profile (the same single binary for all of them; profiles only
# keep the harness's per-profile output directories).  Plain `make amiga`
# builds every supported profile;
# AMIGA_PROFILE selects one for staging or focused development.
AMIGA_PROFILES := wb13 wb31 wb32
AMIGA_PROFILE ?=

amiga:

ifeq ($(AMIGA_PROFILE),)
	@for profile in $(AMIGA_PROFILES); do \
		$(MAKE) -f makefiles/build.mk TARGET=amiga \
			TARGET_BUILD_DIR=build/amiga/$$profile || exit $$?; \
	done
else
	$(MAKE) -f makefiles/build.mk TARGET=amiga \
		TARGET_BUILD_DIR=build/amiga/$(AMIGA_PROFILE)
endif

# Regenerate the checked-in Workbench icon (needs Python; Pillow for preview).
regen-amiga-icons:
	mkdir -p build
	python3 amiga/tools/mklogo.py
	cd amiga && python3 tools/mkinfo.py --preview ../build/amiga-icon-preview.png

# Palm is opt-in and uses the installed NioLib, not the static platform core.
# It builds through the library's launcher: native prc-tools when they are on PATH,
# otherwise the installed palmdev-toolchain container (docs/palmos-config.md).
# NioLib must be built first with the same PALMOS_SDK_VERSION.
PALMOS_SDK_VERSION ?= 3.5
export PALMOS_SDK_VERSION
PALMOS_FUJINET_NIO_LIB = $(or $(FUJINET_NIO_LIB),../fujinet-nio-lib)
PALMOS_NIOLIB_BUILD = $(or $(NIOLIB_BUILD),$(PALMOS_FUJINET_NIO_LIB)/build/palmos-shared-$(PALMOS_SDK_VERSION))
PALMOS_OUT = $(or $(PALMOS_BUILD),build/palmos-usb)
.PHONY: palmos test-palmos trs80 test-trs80-host
palmos:
	bash $(PALMOS_FUJINET_NIO_LIB)/scripts/palmos-build.sh $(MAKE) -f makefiles/palmos.mk FUJINET_NIO_LIB=$(PALMOS_FUJINET_NIO_LIB) NIOLIB_BUILD=$(PALMOS_NIOLIB_BUILD) PALMOS_BUILD=$(PALMOS_OUT)
test-palmos:
	FUJINET_NIO_LIB=$(abspath $(PALMOS_FUJINET_NIO_LIB)) PALMOS_SHARED_BUILD=$(abspath $(PALMOS_NIOLIB_BUILD)) PALMOS_CONFIG_BUILD=$(abspath $(PALMOS_OUT)) \
		$(or $(PALMOS_PYTHON),python3) -m unittest discover -s tests/palmos -v

# TRS-80 Model 4/4P (LS-DOS): opt-in, needs z88dk. Produces trs80/build/config.bin; the /CMD with its client
# is assembled by the boot-disk repo's tools/make_boot_image.py. See docs/trs80-config.md.
trs80:
	$(MAKE) -C trs80 build/config.bin
test-trs80-host:
	$(MAKE) -C tests/trs80 test

# Bootable Workbench floppy with the FujiNet drivers, Config and Install-HD.
# Needs the user's own licensed Workbench floppy; see docs/amiga-bootdisk.md.
#   make amiga-bootdisk WB=wb204 BASE_ADF=/path/to/workbench.adf
# NIO_PKG is the staged package (scripts/amiga-artifacts <profile> in the
# workspace) and CONFIG_NIO the built config-nio (make amiga).
WB ?= wb204
BOOTDISK_PKG_PROFILE_wb204 := wb32
BOOTDISK_PKG_PROFILE_config-only := wb32
BOOTDISK_OUT ?= build/amiga-bootdisk
NIO_PKG ?= $(NIO_WORKSPACE)/build/amiga-artifacts/$(BOOTDISK_PKG_PROFILE_$(WB))/NIO
CONFIG_NIO ?= build/amiga/wb32/bin/config-nio

amiga-bootdisk:
	@test "$(WB)" = config-only || test -n "$(BASE_ADF)" || { echo "BASE_ADF=<your Workbench floppy .adf> is required"; exit 2; }
	python3 amiga/bootdisk/build.py $(WB) $(if $(BASE_ADF),--base "$(BASE_ADF)") --pkg "$(NIO_PKG)" \
		--config-nio "$(CONFIG_NIO)" --out "$(BOOTDISK_OUT)"

test-amiga-bootdisk:
	python3 -I -m unittest discover -s tests/amiga/bootdisk -v
