HOMEPAGE = "http://www.denx.de/wiki/U-Boot/WebHome"
DESCRIPTION = "U-Boot, a boot loader for Embedded boards based on PowerPC, \
ARM, MIPS and several other processors, which can be installed in a boot \
ROM and used to initialize and test the hardware or to download and run \
application code."
SECTION = "bootloaders"
DEPENDS += "flex-native bison-native"
PROVIDES:append = " u-boot"
RPROVIDES:${PN} = "u-boot"

LICENSE = "GPL-2.0-or-later"
LIC_FILES_CHKSUM = "file://Licenses/README;md5=5a7450c57ffe5ae63fd732446b988025"
PE = "1"

S = "${WORKDIR}/git"
B = "${WORKDIR}/build"
do_configure[cleandirs] = "${B}"

require recipes-bsp/u-boot/u-boot.inc
SRC_URI:append = " file://fit_keys/dev.crt file://fit_keys/dev.key"

DEPENDS += "bc-native dtc-native trusted-firmware-a-dtbs"
do_install[depends] += "trusted-firmware-a-dtbs:do_populate_sysroot"

COMPATIBLE_MACHINE ?= "titan-*"

inherit logging

# u-boot.inc deploys "u-boot.bin" by default.
# We also need to deploy the "u-boot" elf binary.
UBOOT_ELF = "u-boot"
UBOOT_ELF_BINARY = "u-boot"

SYSROOT_DIRS:append = " /boot"

STAGING_FIT_KEYS_DIR ?= "${TMPDIR}/work-shared/${MACHINE}/fit-keys"

do_install:prepend() {
    install -m 644 "${STAGING_DIR_HOST}/firmware/${UBOOT_DTB_BINARY}" "${B}/${UBOOT_DTB_BINARY}"
}

do_install:append() {
    install -d "${STAGING_FIT_KEYS_DIR}"
    install -m 644 "${WORKDIR}/fit_keys/dev.crt" "${STAGING_FIT_KEYS_DIR}"
    install -m 644 "${WORKDIR}/fit_keys/dev.key" "${STAGING_FIT_KEYS_DIR}"

    if [ -L ${DEPLOY_DIR_IMAGE}/u-boot-${MACHINE}-${PV}-${PR}.dtb ]; then
        rm ${DEPLOY_DIR_IMAGE}/u-boot-${MACHINE}-${PV}-${PR}.dtb
    fi

    if [ -f ${DEPLOY_DIR_IMAGE}/${UBOOT_DTB_BINARY} ]; then
        rm ${DEPLOY_DIR_IMAGE}/${UBOOT_DTB_BINARY}
    fi

    install -d ${DEPLOY_DIR_IMAGE}
    install -m 644 ${STAGING_DIR_HOST}/firmware/${UBOOT_DTB_BINARY} ${DEPLOY_DIR_IMAGE}/${UBOOT_DTB_BINARY}
    ln -s -r ${DEPLOY_DIR_IMAGE}/${UBOOT_DTB_BINARY} ${DEPLOY_DIR_IMAGE}/u-boot-${MACHINE}-${PV}-${PR}.dtb
}

do_deploy:append() {
    # don't deploy the DTB files, these are deployed by linux recipe
    rm -f ${DEPLOYDIR}/${UBOOT_DTB_BINARY}
    rm -f ${DEPLOYDIR}/u-boot-${MACHINE}*.dtb
}

addtask uboot_assemble_fitimage before do_deploy after do_install

# Add savedefconfig task to u-boot
do_savedefconfig() {
    bbplain "Saving defconfig to:\n${B}/defconfig"
    oe_runmake -C ${B} savedefconfig
}
addtask savedefconfig
