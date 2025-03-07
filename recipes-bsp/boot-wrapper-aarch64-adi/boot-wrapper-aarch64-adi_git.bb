SUMMARY = "ADI Linux aarch64 boot wrapper with FDT support"

inherit oran-boot-wrapper-aarch64-adi
require recipes-bsp/boot-wrapper-aarch64-adi/include/boot-wrapper-aarch64-adi.inc

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=bb63326febfb5fb909226c8e7ebcef5c"
SRCREV = "99bff6a13109568084e4ca48e1bd11753044fb3d"
