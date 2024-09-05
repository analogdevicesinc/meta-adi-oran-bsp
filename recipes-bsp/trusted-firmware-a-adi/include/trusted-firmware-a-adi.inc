SRC_URI ?= "gitsm://git@github.com/analogdevicesinc/oran-trusted-firmware-a.git;protocol=https;branch=main"
SRCREV_FORMAT = "tfa"

# mbedtls is a submodule
TFA_MBEDTLS_DIR = "mbedtls"
