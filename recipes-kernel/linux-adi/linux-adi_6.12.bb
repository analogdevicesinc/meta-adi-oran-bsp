DESCRIPTION = "Linux Kernel for ADI SoCs"
LICENSE = "GPL-2.0-only"

inherit oran-linux-adi
require recipes-kernel/linux-adi/include/linux-adi.inc

LINUX_VERSION = "6.12"

LIC_FILES_CHKSUM = "file://COPYING;md5=6bc538ed5bd9a7fc9398086aedcd7e46"
SRCREV = "ed9340152a82ff6a6ca0f1b6e3d040b8efdc24be"
SRCREV_machine ?= "${SRCREV}"
KBRANCH = "main-6.12.y"
