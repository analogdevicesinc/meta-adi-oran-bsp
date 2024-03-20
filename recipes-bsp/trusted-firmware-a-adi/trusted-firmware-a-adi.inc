DEPENDS:append = " openssl-native"

# Enable warnings as errors (-Werror)
EXTRA_OEMAKE:remove = " E=0"
EXTRA_OEMAKE:append = " E=1"

ADI_CC_BOOT_TEST_FRAMEWORK ?= "0"
EXTRA_OEMAKE:append = " ${@bb.utils.contains('ADI_CC_BOOT_TEST_FRAMEWORK', '1', 'TEST_FRAMEWORK=1 LOG_LEVEL=0', '', d)}"

ADI_CC_BOOT_DEBUG ?= "0"
TFA_DEBUG = "${ADI_CC_BOOT_DEBUG}"

EXTRA_OEMAKE:append = " TRUSTED_BOARD_BOOT=1 GENERATE_COT=1 KEY_ALG=rsa"
EXTRA_OEMAKE:append = " ROT_KEY=plat/adi/adrv/board/${TFA_PLATFORM}/rot_key.pem"
EXTRA_OEMAKE:append = " EL3_EXCEPTION_HANDLING=1 SDEI_SUPPORT=1"

# Configuration for mbedtls
EXTRA_OEMAKE:append = " MBEDTLS_DIR=${TFA_MBEDTLS_DIR}"

# Provide libcrypto.so.3 path
EXTRA_OEMAKE:append = " LD_LIBRARY_PATH=${STAGING_DIR_NATIVE}/usr/lib/"
