DESCRIPTION = "Linux Kernel for ADI SoCs"
LICENSE = "GPL-2.0-only"

inherit oran-linux-adi
require recipes-kernel/linux-adi/include/linux-adi.inc

LINUX_VERSION = "5.10.179"
LIC_FILES_CHKSUM = "file://COPYING;md5=6bc538ed5bd9a7fc9398086aedcd7e46"
SRCREV = "5495ec380e2ef086795e5e3243840a39b155d885"
SRCREV_machine ?= "${SRCREV}"
