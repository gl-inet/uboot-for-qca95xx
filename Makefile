include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-legacy-uboot
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=1.1
PKG_RELEASE:=1

#include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)/$(BUILD_VARIANT)

include $(INCLUDE_DIR)/package.mk

UBOOT_MAKE_OPTS:= \
    LZMA=$(STAGING_DIR_HOST)/bin/lzma \
    ARCH=mips \
    CPU=mips \
    CROSS_COMPILE=$(TARGET_CROSS)

ifdef BUILD_VARIANT
UBOOT_CONFIG:=$(patsubst UBOOT_CONFIG_TARGET=%,%,\
        $(filter UBOOT_CONFIG_TARGET%,\
            $(shell cat ./configs/$(BUILD_VARIANT)-$(BOARD)$(if $(SUBTARGET),_$(SUBTARGET)).mk)))
UBOOT_IMAGE:=$(if $(IMAGE),$(IMAGE),uboot-gl-$(BUILD_VARIANT).bin)
UBOOT_MAKE_OPTS+=$(patsubst MAKEOPTS_%,%,\
        $(filter MAKEOPTS_%,\
            $(shell cat ./configs/$(BUILD_VARIANT)-$(BOARD)$(if $(SUBTARGET),_$(SUBTARGET)).mk)))
endif


define Build/Prepare
#	$(call Build/Prepare/Default)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Build/Configure
	($(foreach var,$(strip $(UBOOT_MAKE_OPTS)), export $(var);) \
		$(MAKE) -C $(PKG_BUILD_DIR) mrproper);
	($(foreach var,$(strip $(UBOOT_MAKE_OPTS)), export $(var);) \
		$(MAKE) -C $(PKG_BUILD_DIR) $(UBOOT_CONFIG)_config \
			BUILDVERSION=$(PKG_VERSION)-$(PKG_RELEASE));
endef

define Build/Compile
	($(foreach var,$(strip $(UBOOT_MAKE_OPTS)), export $(var);) \
		$(MAKE) -C $(PKG_BUILD_DIR) all);
endef

define Build/InstallDev
endef

define Package/qca-legacy-uboot/common
  define Package/GL-uboot-$(1)
    SECTION:=boot
    CATEGORY:=Boot Loaders
    TITLE:=U-boot for $(1)
    DEPENDS:=@TARGET_ar71xx $(2)
    URL:=http://www.denx.de/wiki/U-Boot
    VARIANT:=$(1)
  endef
endef

define Package/qca-legacy-uboot/nor
  $(call Package/qca-legacy-uboot/common,$(1),$(2))

  define Package/GL-uboot-$(1)/install
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/u-boot.bin $(BIN_DIR)/$(UBOOT_IMAGE)
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/tools/spi_prog/prog_free.bin $(BIN_DIR)/
  endef

  $$(eval $$(call BuildPackage,GL-uboot-$(1)))
endef

define Package/qca-legacy-uboot/nand
  $(call Package/qca-legacy-uboot/common,$(1),+qca-romboot-$(2))

  define Package/GL-uboot-$(1)/install
	UTILPATH=$(STAGING_DIR_HOST)/bin $(STAGING_DIR_HOST)/bin/mk2stage-$(2) \
		-1 $(STAGING_DIR)/boot/openwrt-$(BOARD)-$(2)-rombootdrv.bin \
		-2 $(PKG_BUILD_DIR)/u-boot.bin \
		-o $(BIN_DIR)/$(patsubst %.bin,%.2fw,$(UBOOT_IMAGE))
  endef

  $$(eval $$(call BuildPackage,GL-uboot-$(1)))
endef




#$(eval $(call Package/qca-legacy-uboot/nor,ap121))
#$(eval $(call Package/qca-legacy-uboot/nor,ap135))
#$(eval $(call Package/qca-legacy-uboot/nand,ap135-nand,qca955x))
#$(eval $(call Package/qca-legacy-uboot/nor,ap136))
#$(eval $(call Package/qca-legacy-uboot/nor,ap137-16M))
#$(eval $(call Package/qca-legacy-uboot/nor,db12x))
#$(eval $(call Package/qca-legacy-uboot/nand,cus227,ar934x))
#$(eval $(call Package/qca-legacy-uboot/nor,db12x-16M))
#$(eval $(call Package/qca-legacy-uboot/nor,ap143-16M))
#$(eval $(call Package/qca-legacy-uboot/nor,ap143-32M))
#$(eval $(call Package/qca-legacy-uboot/nor,ap147-8M))
#$(eval $(call Package/qca-legacy-uboot/nor,ap147-16M))
#$(eval $(call Package/qca-legacy-uboot/nor,ap151-8M))
#$(eval $(call Package/qca-legacy-uboot/nor,ap151-16M))
#$(eval $(call Package/qca-legacy-uboot/nor,ap151-32M))
#$(eval $(call Package/qca-legacy-uboot/nor,ap152-8M))
$(eval $(call Package/qca-legacy-uboot/nor,ar750s))
#$(eval $(call Package/qca-legacy-uboot/nor,ap152-32M))
#$(eval $(call Package/qca-legacy-uboot/nor,ap152-dual))
#$(eval $(call Package/qca-legacy-uboot/nor,cus531-16M))
#$(eval $(call Package/qca-legacy-uboot/nor,cus531-32M))
#$(eval $(call Package/qca-legacy-uboot/nor,cus531-dual))
$(eval $(call Package/qca-legacy-uboot/nor,ar750))
$(eval $(call Package/qca-legacy-uboot/nor,x750-4g))
$(eval $(call Package/qca-legacy-uboot/nor,ar300m))
$(eval $(call Package/qca-legacy-uboot/nor,mifi-v3))
$(eval $(call Package/qca-legacy-uboot/nor,x300b))
$(eval $(call Package/qca-legacy-uboot/nor,xe300))
$(eval $(call Package/qca-legacy-uboot/nor,x1200))

