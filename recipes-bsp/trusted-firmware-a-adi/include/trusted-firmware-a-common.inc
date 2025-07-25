require recipes-bsp/trusted-firmware-a/trusted-firmware-a.inc

MACHINE_TFA_REQUIRE ?= ""

DEPENDS:append = " openssl-native"

ADI_CC_BOOT_TEST_FRAMEWORK ?= "0"
ADI_CC_BOOT_DEBUG ?= "0"

ROT_KEY ?= "plat/adi/adrv/board/${TFA_PLATFORM}/rot_key.pem"

TFA_DEBUG = "${ADI_CC_BOOT_DEBUG}"

EXTRA_OEMAKE:append = " ${@bb.utils.contains('ADI_CC_BOOT_TEST_FRAMEWORK', '1', 'TEST_FRAMEWORK=1 LOG_LEVEL=0', '', d)}"
EXTRA_OEMAKE:append = " TFA_DEBUG=${TFA_DEBUG} TF_A_DIR=."

EXTRA_OEMAKE:append = " TRUSTED_BOARD_BOOT=1 GENERATE_COT=1 KEY_ALG=rsa"
EXTRA_OEMAKE:append = ' ROT_KEY="\"${ROT_KEY}\""'
EXTRA_OEMAKE:append = " EL3_EXCEPTION_HANDLING=1 SDEI_SUPPORT=1"
EXTRA_OEMAKE:append = " TFW_NVCTR_VAL=${ANTI_ROLLBACK_VERSION} NTFW_NVCTR_VAL=${ANTI_ROLLBACK_VERSION}"
EXTRA_OEMAKE:append = " APP_PACK_ANTI_ROLLBACK_VERSION=${APP_PACK_ANTI_ROLLBACK_VERSION}"
EXTRA_OEMAKE:append = " RMA_CLI=${RMA_CLI}"
EXTRA_OEMAKE:append = " SECONDARY_LINUX_ENABLED=${SECONDARY_LINUX_ENABLED}"

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

def get_openssl_engines(d):
    key = d.getVar('ROT_KEY', expand=True)
    val = ''
    if key.startswith('pkcs11:'):
        val += d.getVar('RECIPE_SYSROOT_NATIVE') + '/usr/lib/engines-3/'
    return val

def get_pkcs11_module(d):
    key = d.getVar('ROT_KEY', expand=True)
    val = ''
    if key.startswith('pkcs11:'):
        val += d.getVar('RECIPE_SYSROOT_NATIVE') + '/usr/lib/pkcs11/p11-kit-client.so'
    return val

def get_extra_depends(d):
    key = d.getVar('ROT_KEY', expand=True)
    val = ''
    if key.startswith('pkcs11:'):
        val += 'libp11-native p11-kit-native'
    return val

DEPENDS:append = " ${@get_extra_depends(d)}"

export OPENSSL_ENGINES="${@get_openssl_engines(d)}"
export PKCS11_MODULE_PATH="${@get_pkcs11_module(d)}"
export P11_KIT_SERVER_ADDRESS
export P11_KIT_SERVER_PID
