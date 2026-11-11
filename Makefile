TARGETS := msdos bbc master linux atari
DEFAULT_TARGET := $(if $(TARGET),$(TARGET),all-targets)

.PHONY: atari-banked all all-targets clean disk confnio-bbc-disk confnio-master-disk \
	splash-bbc splash-bbc-disk test-amiga-host amiga regen-amiga-icons $(TARGETS)

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
# Workbench profile.  Plain `make amiga` builds every supported profile;
# AMIGA_PROFILE selects one for staging or focused development.
AMIGA_PROFILES := wb13 wb31 wb32
AMIGA_PROFILE ?=
AMIGA_CRT_wb13 := nix13
AMIGA_CRT_wb31 := clib2
AMIGA_CRT_wb32 := clib2

amiga:

ifeq ($(AMIGA_PROFILE),)
	@for profile in $(AMIGA_PROFILES); do \
		$(MAKE) -f makefiles/build.mk TARGET=amiga \
			TARGET_BUILD_DIR=build/amiga/$$profile \
			AMIGA_CRT=$$(if [ "$$profile" = wb13 ]; then echo nix13; else echo clib2; fi) \
			AMIGA_WB13=$$(if [ "$$profile" = wb13 ]; then echo 1; else echo 0; fi) || exit $$?; \
	done
else
	$(MAKE) -f makefiles/build.mk TARGET=amiga \
		TARGET_BUILD_DIR=build/amiga/$(AMIGA_PROFILE) \
		AMIGA_CRT=$(AMIGA_CRT_$(AMIGA_PROFILE)) \
		AMIGA_WB13=$(if $(filter wb13,$(AMIGA_PROFILE)),1,0)
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
.PHONY: palmos test-palmos
palmos:
	bash $(PALMOS_FUJINET_NIO_LIB)/scripts/palmos-build.sh $(MAKE) -f makefiles/palmos.mk FUJINET_NIO_LIB=$(PALMOS_FUJINET_NIO_LIB) NIOLIB_BUILD=$(PALMOS_NIOLIB_BUILD) PALMOS_BUILD=$(PALMOS_OUT)
test-palmos:
	FUJINET_NIO_LIB=$(abspath $(PALMOS_FUJINET_NIO_LIB)) PALMOS_SHARED_BUILD=$(abspath $(PALMOS_NIOLIB_BUILD)) PALMOS_CONFIG_BUILD=$(abspath $(PALMOS_OUT)) \
		$(or $(PALMOS_PYTHON),python3) -m unittest discover -s tests/palmos -v
