require optee-os-common.inc
PROVIDES:append = " optee-os"
RPROVIDES:${PN} = "optee-os"

DEPENDS:append = " optee-apps"
do_compile[depends] += " optee-apps:do_install"

do_compile() {
    EARLY_TA_ELFS=`ls ${ADI_EARLY_TAS_DEPLOY_DIR}/*.stripped.elf`
    oe_runmake EARLY_TA_PATHS=${EARLY_TA_ELFS} -C ${S} all
}

# Debug image configurations
ADI_BOOT_DEBUG_CONFIGS = "\
    CFG_WARN_INSECURE=y \
    CFG_LOCKDEP_RECORD_STACK=y \
    CFG_UNWIND=y \
    CFG_DEBUG_INFO=y \
    CFG_TEE_CORE_DEBUG=y \
    CFG_TEE_CORE_TA_TRACE=y \
    CFG_ADI_ADIMEM_PTA=y \
    "
EXTRA_OEMAKE:append = " ${@bb.utils.contains('ADI_CC_BOOT_DEBUG','1' ,'${ADI_BOOT_DEBUG_CONFIGS}', ' ', d)}"
