SRC_URI = "gitsm://git@github.com/analogdevicesinc/oran-trusted-firmware-a.git;protocol=https;branch=main"
SRCREV = "bb6a660a4a05bf52056ee2b0ba0458ae4147ccd7"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://docs/license.rst;md5=b2c740efedc159745b9b31f88ff03dde"
SRCREV_FORMAT = "tfa"
# mbedtls is a submodule
TFA_MBEDTLS_DIR = "mbedtls"
LIC_FILES_CHKSUM:append = " file://${TFA_MBEDTLS_DIR}/LICENSE;md5=3b83ef96387f14655fc854ddc3c6bd57"