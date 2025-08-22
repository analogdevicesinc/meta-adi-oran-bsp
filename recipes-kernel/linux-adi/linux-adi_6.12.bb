DESCRIPTION = "Linux Kernel for ADI SoCs"
LICENSE = "GPL-2.0-only"

inherit oran-linux-adi
require recipes-kernel/linux-adi/include/linux-adi.inc

LINUX_VERSION = "6.12"

LIC_FILES_CHKSUM = "file://COPYING;md5=6bc538ed5bd9a7fc9398086aedcd7e46"
SRCREV = "ec96aa47ab7e1a9ac3da1f1395478fc84e07afe2"
SRCREV_machine ?= "${SRCREV}"
KBRANCH = "main-6.12.y"
