CONFIG_NIO_BBC_ASSET_DIR ?= bbc/assets
CONFIG_NIO_BBC_STAGE_DIR ?= $(DISK_DIR)/config-nio
CONFIG_NIO_BBC_SPLASH_DIR ?= src/platform/bbc/splash
CONFIG_NIO_BBC_SPLASH_BUILD_DIR ?= build/bbc/splash

.PHONY: config-nio-bbc-stage config-nio-bbc-boot-stage config-nio-master-stage config-nio-master-boot-stage

define CONFIG_NIO_STAGE_PROFILE
	$(MAKE) -f makefiles/build.mk \
		TARGET=$(1) \
		BUILD_DIR="$(BUILD_DIR)/config-nio-$(2)" \
		FUJINET_NIO_LIB="$(FUJINET_NIO_LIB)" \
		BBC_CONFIG_NIO_START_ADDRESS="$(3)" \
		BBC_CONFIG_NIO_HIMEM="$(4)" \
		BBC_CONFIG_NIO_SHADOW_MODE="$(5)" \
		BBC_CONFIG_NIO_XRAM_TABLES="$(6)" \
		clean config-nio keycode
	rm -rf "$(CONFIG_NIO_BBC_STAGE_DIR)"
	mkdir -p "$(CONFIG_NIO_BBC_STAGE_DIR)"
	cp "$(BUILD_DIR)/config-nio-$(2)/$(1)/bin/config-nio" "$(CONFIG_NIO_BBC_STAGE_DIR)/CONFNIO"
	cp "$(CONFIG_NIO_BBC_ASSET_DIR)/config-nio-$(2)/CONFNIO.inf" "$(CONFIG_NIO_BBC_STAGE_DIR)/CONFNIO.inf"
	cp "$(BUILD_DIR)/config-nio-$(2)/$(1)/bin/keycode" "$(CONFIG_NIO_BBC_STAGE_DIR)/KEYCODE"
	cp "$(CONFIG_NIO_BBC_ASSET_DIR)/KEYCODE.inf" "$(CONFIG_NIO_BBC_STAGE_DIR)/KEYCODE.inf"
endef

config-nio-bbc-stage:
	$(call CONFIG_NIO_STAGE_PROFILE,bbc,bbc,0x1900,0x7C00,0,0)

config-nio-bbc-boot-stage: config-nio-bbc-stage
	$(MAKE) -C "$(CONFIG_NIO_BBC_SPLASH_DIR)" config CONFIG_MASTER=
	cp "$(CONFIG_NIO_BBC_SPLASH_BUILD_DIR)/CONFIG" "$(CONFIG_NIO_BBC_STAGE_DIR)/CONFIG"
	cp "$(CONFIG_NIO_BBC_ASSET_DIR)/config-nio-bbc/CONFIG.inf" "$(CONFIG_NIO_BBC_STAGE_DIR)/CONFIG.inf"
	cp "$(CONFIG_NIO_BBC_SPLASH_BUILD_DIR)/SCREEN" "$(CONFIG_NIO_BBC_STAGE_DIR)/SCREEN"
	cp "$(CONFIG_NIO_BBC_ASSET_DIR)/config-nio-bbc/SCREEN.inf" "$(CONFIG_NIO_BBC_STAGE_DIR)/SCREEN.inf"
	tr '\n' '\r' < "$(CONFIG_NIO_BBC_ASSET_DIR)/config-nio-bbc/!BOOT" > "$(CONFIG_NIO_BBC_STAGE_DIR)/!BOOT"
	printf '$$.!BOOT 000000 000000\n' > "$(CONFIG_NIO_BBC_STAGE_DIR)/!BOOT.inf"

config-nio-master-boot-stage: config-nio-master-stage
	$(MAKE) -C "$(CONFIG_NIO_BBC_SPLASH_DIR)" config CONFIG_MASTER=1
	cp "$(CONFIG_NIO_BBC_SPLASH_BUILD_DIR)/CONFIG" "$(CONFIG_NIO_BBC_STAGE_DIR)/CONFIG"
	cp "$(CONFIG_NIO_BBC_ASSET_DIR)/config-nio-master/CONFIG.inf" "$(CONFIG_NIO_BBC_STAGE_DIR)/CONFIG.inf"
	cp "$(CONFIG_NIO_BBC_SPLASH_BUILD_DIR)/SCREEN" "$(CONFIG_NIO_BBC_STAGE_DIR)/SCREEN"
	cp "$(CONFIG_NIO_BBC_ASSET_DIR)/config-nio-master/SCREEN.inf" "$(CONFIG_NIO_BBC_STAGE_DIR)/SCREEN.inf"
	tr '\n' '\r' < "$(CONFIG_NIO_BBC_ASSET_DIR)/config-nio-master/!BOOT" > "$(CONFIG_NIO_BBC_STAGE_DIR)/!BOOT"
	printf '$$.!BOOT 000000 000000\n' > "$(CONFIG_NIO_BBC_STAGE_DIR)/!BOOT.inf"

config-nio-master-stage:
	$(call CONFIG_NIO_STAGE_PROFILE,master,master,0x0E00,0x8000,1,1)
