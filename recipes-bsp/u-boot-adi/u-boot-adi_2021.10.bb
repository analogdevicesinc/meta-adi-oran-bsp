HOMEPAGE = "http://www.denx.de/wiki/U-Boot/WebHome"
DESCRIPTION = "U-Boot, a boot loader for Embedded boards based on PowerPC, \
ARM, MIPS and several other processors, which can be installed in a boot \
ROM and used to initialize and test the hardware or to download and run \
application code."
SECTION = "bootloaders"

inherit oran-u-boot-adi uboot-sign-adi
require recipes-bsp/u-boot-adi/include/u-boot-adi.inc

LICENSE = "GPL-2.0-or-later"
LIC_FILES_CHKSUM = "file://Licenses/README;md5=5a7450c57ffe5ae63fd732446b988025"
SRCREV ?= "e5c4168183076ab4b85271796c5d03a565442eb7"
