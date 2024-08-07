require recipes-bsp/trusted-firmware-a/trusted-firmware-a.inc

MACHINE_TFA_REQUIRE ?= ""

DEPENDS:append = " openssl-native"

ADI_CC_BOOT_TEST_FRAMEWORK ?= "0"
ADI_CC_BOOT_DEBUG ?= "0"

ROT_KEY ?= "plat/adi/adrv/board/${TFA_PLATFORM}/rot_key.pem"

TFA_DEBUG = "${ADI_CC_BOOT_DEBUG}"

# Enable warnings as errors (-Werror)
EXTRA_OEMAKE:remove = " E=0"
EXTRA_OEMAKE:append = " E=1"

EXTRA_OEMAKE:append = " ${@bb.utils.contains('ADI_CC_BOOT_TEST_FRAMEWORK', '1', 'TEST_FRAMEWORK=1 LOG_LEVEL=0', '', d)}"

EXTRA_OEMAKE:append = " TRUSTED_BOARD_BOOT=1 GENERATE_COT=1 KEY_ALG=rsa"
EXTRA_OEMAKE:append = " ROT_KEY=${ROT_KEY}"
EXTRA_OEMAKE:append = " EL3_EXCEPTION_HANDLING=1 SDEI_SUPPORT=1"
EXTRA_OEMAKE:append = " TFW_NVCTR_VAL=${ANTI_ROLLBACK_VERSION} NTFW_NVCTR_VAL=${ANTI_ROLLBACK_VERSION}"
EXTRA_OEMAKE:append = " RMA_CLI=${RMA_CLI}"

# Configuration for mbedtls
EXTRA_OEMAKE:append = " MBEDTLS_DIR=${TFA_MBEDTLS_DIR}"

# Provide libcrypto.so.3 path
EXTRA_OEMAKE:append = " LD_LIBRARY_PATH=${STAGING_DIR_NATIVE}/usr/lib/"

# Security hardening configuration (suggested by: https://trustedfirmware-a.readthedocs.io/en/latest/process/security-hardening.html)
SECURITY_HARDENING_CONFIGS = "\
    BRANCH_PROTECTION=4 \
    ENABLE_STACK_PROTECTOR=default \
    "
EXTRA_OEMAKE:append = " ${SECURITY_HARDENING_CONFIGS}"
