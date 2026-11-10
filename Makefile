TARGETS := msdos bbc master linux atari c64
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
