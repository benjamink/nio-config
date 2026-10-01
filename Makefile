TARGETS := msdos bbc master linux
DEFAULT_TARGET := $(if $(TARGET),$(TARGET),all-targets)

.PHONY: all all-targets clean disk confnio-bbc-disk confnio-master-disk \
	splash-bbc splash-bbc-disk test-amiga-host amiga regen-amiga-icons $(TARGETS)

all: $(DEFAULT_TARGET)

$(TARGETS):
	$(MAKE) -f makefiles/build.mk TARGET=$@

all-targets: $(TARGETS)

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
