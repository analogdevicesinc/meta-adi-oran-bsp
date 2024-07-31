LICENSE = "BSD-3-Clause"
COMPATIBLE_MACHINE = "titan|denali"

inherit oran-trusted-firmware-a-adi
require recipes-bsp/trusted-firmware-a-adi/include/trusted-firmware-a-adi.inc
require recipes-bsp/trusted-firmware-a-adi/include/trusted-firmware-a-adi_git.inc

LIC_FILES_CHKSUM = "file://docs/license.rst;md5=b2c740efedc159745b9b31f88ff03dde"
LIC_FILES_CHKSUM:append = " file://${TFA_MBEDTLS_DIR}/LICENSE;md5=3b83ef96387f14655fc854ddc3c6bd57"
