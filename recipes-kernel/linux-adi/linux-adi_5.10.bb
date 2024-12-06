DESCRIPTION = "Linux Kernel for ADI SoCs"
LICENSE = "GPL-2.0-only"

inherit oran-linux-adi
require recipes-kernel/linux-adi/include/linux-adi.inc

LINUX_VERSION = "5.10.179"
LIC_FILES_CHKSUM = "file://COPYING;md5=6bc538ed5bd9a7fc9398086aedcd7e46"
SRCREV = "dfc61e5ab19b2418877d4cfe7d83e60287d58803"
SRCREV_machine ?= "${SRCREV}"
