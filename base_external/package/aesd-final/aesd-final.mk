
##############################################################
#
# AESD-FINAL
#
##############################################################

AESD_FINAL_OVERRIDE_SRCDIR=../base_external/package/aesd-final/source
AESD_FINAL_VERSION = v1.0
AESD_FINAL_SITE = 'git@github.com:cu-ecen-5013/final-app-rmuchmore.git'
AESD_FINAL_SITE_METHOD = git

define AESD_FINAL_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D) all
endef

# Install required executables and/or scripts
define AESD_ASSIGNMENTS_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 $(@D)/vehicle/vehicle $(TARGET_DIR)/bin
	$(INSTALL) -m 0755 $(@D)/scan_tool/scan_tool $(TARGET_DIR)/bin
endef

$(eval $(generic-package))
