SUMMARY = "ADI Linux aarch64 boot wrapper with FDT support"

inherit oran-boot-wrapper-aarch64-adi
require recipes-bsp/boot-wrapper-aarch64-adi/include/boot-wrapper-aarch64-adi.inc

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=bb63326febfb5fb909226c8e7ebcef5c"
SRCREV ?= "bb1fe528a80f8aeca71742c2fb2b002890381613"
