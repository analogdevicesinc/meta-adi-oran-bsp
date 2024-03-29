SUMMARY = "OP-TEE Trusted OS TA devkit"
DESCRIPTION = "OP-TEE TA devkit for build TAs"
HOMEPAGE = "https://www.op-tee.org/"
PROVIDES:append = " optee-os-tadevkit"
RPROVIDES:${PN} = "optee-os-tadevkit"

require optee-os-common.inc

EXTRA_OEMAKE += " CFG_EARLY_TA=y "

do_install() {
    #install TA devkit
    install -d ${D}${includedir}/optee/export-user_ta/
    for f in ${B}/export-ta_${OPTEE_ARCH}/* ; do
        cp -aR $f ${D}${includedir}/optee/export-user_ta/
    done
}

do_deploy() {
    echo "Do not inherit do_deploy from optee-os."
}

FILES:${PN} = "${includedir}/optee/"