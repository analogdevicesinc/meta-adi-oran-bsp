DEPENDS += "flex-native bison-native"
PROVIDES:append = " u-boot"
RPROVIDES:${PN} = "u-boot"

PE = "1"

B = "${WORKDIR}/build"
do_configure[cleandirs] = "${B}"

require recipes-bsp/u-boot/u-boot.inc

# Enable warnings-as-errors
EXTRA_OEMAKE:append = " KCFLAGS="-Werror""

DEPENDS += "bc-native dtc-native trusted-firmware-a-dtbs"
do_install[depends] += "trusted-firmware-a-dtbs:do_populate_sysroot"

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
