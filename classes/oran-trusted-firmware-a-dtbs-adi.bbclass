require recipes-bsp/trusted-firmware-a-adi/include/trusted-firmware-a-common.inc

PROVIDES:append = " trusted-firmware-a-dtbs"

TFA_BUILD_TARGET = "dtbs"
TFA_INSTALL_TARGET = "${TFA_HW_CONFIG} ${TFA_FW_CONFIG}"

do_compile() {
    # override ARM's do_compile because they try to patch files during this step

    # Currently there are races if you build all the targets at once in parallel
    for T in ${TFA_BUILD_TARGET}; do
        oe_runmake -C ${S} $T
    done
}

do_deploy() {
}
