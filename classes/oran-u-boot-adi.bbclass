DEPENDS += "flex-native bison-native"
PROVIDES:append = " u-boot"
RPROVIDES:${PN} = "u-boot"

PE = "1"

B = "${WORKDIR}/build"
do_configure[cleandirs] = "${B}"

require recipes-bsp/u-boot/u-boot.inc

# Enable warnings-as-errors

DEPENDS += "bc-native dtc-native trusted-firmware-a-dtbs"
do_install[depends] += "trusted-firmware-a-dtbs:do_populate_sysroot"

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

# Add savedefconfig task to u-boot
do_savedefconfig() {
    bbplain "Saving defconfig to:\n${B}/defconfig"
    oe_runmake -C ${B} savedefconfig
}
addtask savedefconfig

do_uboot_assemble_fitimage[depends] += "virtual/kernel:do_assemble_fitimage"
do_uboot_assemble_fitimage:append() {
    # mkimage seems to add nearly 2K of padding, putting us over the 8K limit.
    # we can shed this padding by de-compiling and re-compiling the device tree.
    dtc -I dtb -O dts "${B}/${UBOOT_DTB_BINARY}" > "${B}/${UBOOT_DTB_BINARY}.dts"
    dtc -I dts -O dtb "${B}/${UBOOT_DTB_BINARY}.dts" > "${B}/${UBOOT_DTB_BINARY}"

    cp -P "${B}/${UBOOT_DTB_BINARY}" "${B}/u-boot-${MACHINE}.dtb"

    install -Dm 644 "${B}/${UBOOT_DTB_BINARY}" "${TMPDIR}/work-shared/${MACHINE}/${UBOOT_DTB_BINARY}"
}
