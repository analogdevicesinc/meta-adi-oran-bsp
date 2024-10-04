SRC_URI = "git://git@github.com/analogdevicesinc/oran-u-boot.git;protocol=ssh;branch=main"
SRC_URI:append = " file://fit_keys/dev.crt file://fit_keys/dev.key"
SRC_URI:append = "${@bb.utils.contains('ADI_CC_BOOT_DEBUG','1' ,' file://u-boot-adi-debug.cfg', ' ', d)}"
S = "${WORKDIR}/git"
