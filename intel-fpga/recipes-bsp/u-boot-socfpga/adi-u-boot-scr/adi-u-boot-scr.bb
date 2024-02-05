DESCRIPTION = "Boot script for launching images with U-Boot distro boot"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/COPYING.MIT;md5=3da9cfbcb788c80a0384361b4de20420"

INHIBIT_DEFAULT_DEPS = "1"
DEPENDS = "u-boot-mkimage-native dos2unix"

FILESEXTRAPATHS:prepend := "${THISDIR}:"

UBOOT_TXT_FILE = "${MACHINE}-u-boot.txt"

SRC_URI:append = " file://files/${UBOOT_TXT_FILE}"

inherit deploy

do_deploy() {
    #TODO: checked-in the Linux format file, rather using the dos2unix command below(at customer release stage)
    /usr/bin/dos2unix ${WORKDIR}/files/${UBOOT_TXT_FILE}

    mkimage -A arm -O linux -T script -C none -a 0 -e 0 -n "ADI u-boot script" -d ${WORKDIR}/files/${UBOOT_TXT_FILE} u-boot.scr
    install -m 0644 u-boot.scr ${DEPLOYDIR}/u-boot.scr-${MACHINE}
    ln -s u-boot.scr-${MACHINE} ${DEPLOYDIR}/u-boot.scr
}

addtask do_deploy after do_install before do_build

PROVIDES += "u-boot-default-script"

PACKAGE_ARCH = "${MACHINE_ARCH}"

