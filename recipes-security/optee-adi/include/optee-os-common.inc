require optee-common.inc

# from meta-arm
require recipes-security/optee/optee-os.inc

require optee-uris-adi.inc
SRC_URI = "${OPTEE_OS_URI};protocol=${OPTEE_OS_PROT};branch=${OPTEE_OS_BRANCH}"

COMPATIBLE_MACHINE = ""

EXTRA_OEMAKE:append = " PLATFORM_FLAVOR=${PLATFORM_FLAVOR}"
EXTRA_OEMAKE:append = " ${@bb.utils.contains('ADI_CC_BOOT_DEBUG', '1', 'DEBUG=1 CFG_TEE_CORE_LOG_LEVEL=2', '', d)}"
EXTRA_OEMAKE:append = " CROSS_COMPILE=${HOST_PREFIX}"
EXTRA_OEMAKE:append = " CROSS_COMPILE_core=${HOST_PREFIX}"
EXTRA_OEMAKE:append = " CROSS_COMPILE_ta_arm32=${HOST_PREFIX}"
EXTRA_OEMAKE:append = " CROSS_COMPILE_ta_arm64=${HOST_PREFIX}"
EXTRA_OEMAKE:append = " PYTHON3=${PYTHON}"

# Default Keys
DEPENDS:append = " optee-keys-native"
DEFAULT_TA_SIGNING_PUB_KEY_PATH = "${RECIPE_SYSROOT_NATIVE}${datadir}/default-optee-keys/optee_pub_key.pem"
# Used Keys
TA_SIGNING_PUB_KEY_PATH ??= "${DEFAULT_TA_SIGNING_PUB_KEY_PATH}"
EXTRA_OEMAKE:append = " TA_PUBLIC_KEY=${TA_SIGNING_PUB_KEY_PATH}"

ADI_EARLY_TAS_DEPLOY_DIR = "${STAGING_DIR}/optee-apps-early-tas"
