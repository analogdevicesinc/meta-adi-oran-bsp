DESCRIPTION = "Linux Kernel for ADI SoCs"
LICENSE = "GPL-2.0-only"


KERNEL_REPO ?= "git://git@github.com/analogdevicesinc/oran-linux.git"
KERNEL_PROT ?= "https"
KBRANCH ?= "main"

# Build branch name
LINUX_VERSION_PREFIX ?= "adi-"
LINUX_VERSION_SUFFIX ?= ""

SRCREV ?= "${AUTOREV}"
SRCREV_machine ?= "${AUTOREV}"
PV = "${LINUX_VERSION}${LINUX_VERSION_SUFFIX}"
PV:append = "+git${SRCPV}"

SRC_URI = "${KERNEL_REPO};protocol=${KERNEL_PROT};branch=${KBRANCH}"

require recipes-kernel/linux/linux-yocto.inc

DEPENDS:append = " trusted-firmware-a-dtbs"

KCONFIG_MODE ?= "--alldefconfig"

KERNEL_IMAGETYPE ?= "Image"

# Deploy vmlinux for ArmDS
KERNEL_IMAGETYPES:append = " vmlinux"

KERNEL_EXTRA_ARGS:append = " KCFLAGS=-Werror"

do_install:append() {
    for dtb in ${KERNEL_DEVICETREE}; do
        install -Dm 644 "${B}/arch/arm64/boot/dts/${dtb}" "${D}/boot/fdt/$(basename "$dtb")"
    done
}

do_assemble_fitimage[depends] += "trusted-firmware-a-dtbs:do_populate_sysroot virtual/bootloader:do_install"
do_assemble_fitimage:prepend() {
    cp -P "${STAGING_DIR_HOST}/firmware/${UBOOT_DTB_BINARY}" "${B}"
}

do_uboot_assemble_fitimage[depends] += "virtual/kernel:do_assemble_fitimage"
do_uboot_assemble_fitimage:append() {
    # mkimage seems to add nearly 2K of padding, putting us over the 8K limit.
    # we can shed this padding by de-compiling and re-compiling the device tree.
    dtc -I dtb -O dts "${B}/${UBOOT_DTB_BINARY}" > "${B}/${UBOOT_DTB_BINARY}.dts"
    dtc -I dts -O dtb "${B}/${UBOOT_DTB_BINARY}.dts" > "${B}/${UBOOT_DTB_BINARY}"

    cp -P "${B}/${UBOOT_DTB_BINARY}" "${B}/u-boot-${MACHINE}.dtb"

    install -Dm 644 "${B}/${UBOOT_DTB_BINARY}" "${TMPDIR}/work-shared/${MACHINE}/${UBOOT_DTB_BINARY}"
}

do_assemble_fitimage_initramfs[depends] += "virtual/kernel:do_assemble_fitimage"
addtask assemble_fitimage_initramfs before do_deploy after do_uboot_assemble_fitimage

do_deploy:append() {
    cp ${DEPLOYDIR}/fitImage-${INITRAMFS_IMAGE}-${MACHINE}-${MACHINE} \
    ${DEPLOYDIR}/${KERNEL_FIT_IMAGE}
}

SYSROOT_DIRS:append = " /boot/fdt"
