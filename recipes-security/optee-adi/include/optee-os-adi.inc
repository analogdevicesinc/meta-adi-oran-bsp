require optee-os-common.inc
PROVIDES:append = " optee-os"
RPROVIDES:${PN} = "optee-os"

DEPENDS:append = " optee-apps"
do_compile[depends] += " optee-apps:do_install"

do_compile() {
    EARLY_TA_ELFS=`ls ${ADI_EARLY_TAS_DEPLOY_DIR}/*.stripped.elf`
    oe_runmake EARLY_TA_PATHS=${EARLY_TA_ELFS} -C ${S} all
}