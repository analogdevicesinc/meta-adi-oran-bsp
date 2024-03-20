require recipes-bsp/trusted-firmware-a/trusted-firmware-a.inc
require trusted-firmware-a-adi_git.inc
require trusted-firmware-a-adi.inc

PROVIDES:append = " trusted-firmware-a"
RPROVIDES:${PN} = "trusted-firmware-a"

MACHINE_TFA_REQUIRE ?= ""

COMPATIBLE_MACHINE = "titan-*"

#
# Secondary App-Pack
#

DEPENDS:append  = " secondary-app-pack"
do_compile[depends] = " secondary-app-pack:do_install"

TFA_SECONDARY_IMAGE_BIN = "${DEPLOY_DIR}/app-pack/secondary_app_pack.bin"
EXTRA_OEMAKE:append = " SECONDARY_IMAGE_BIN=${TFA_SECONDARY_IMAGE_BIN}"

#
# U-Boot Configuration
#

TFA_UBOOT ="1"
DEPENDS += "u-boot"
do_compile[depends] += " u-boot:do_install"
EXTRA_OEMAKE:append = " BL33=${STAGING_DIR_HOST}/boot/u-boot.bin"

#
# OP-TEE Configuration
#

DEPENDS:append  = " optee-os"
do_compile[depends] += " optee-os:do_deploy"
TFA_SPD= "opteed"
EXTRA_OEMAKE:append = " BL32=${DEPLOY_DIR_IMAGE}/optee/tee-pager_v2.bin"

#
# Device Tree Configuration
#

DEPENDS:append  = " virtual/kernel"
do_compile[depends] += " trusted-firmware-a-dtbs:do_populate_sysroot virtual/kernel:do_uboot_assemble_fitimage"
EXTRA_OEMAKE:append = " HW_CONFIG=${TMPDIR}/work-shared/${MACHINE}/${UBOOT_DTB_BINARY}"
EXTRA_OEMAKE:append = " FW_CONFIG=${STAGING_DIR_HOST}/firmware/${TFA_FW_CONFIG}.dtb"

#build without floating point support
NON_FP_BUILD_TARGET = "bl1 bl31 fip"

#build with floating point support
TFA_BUILD_TARGET ="bl2 "

#to install
TFA_INSTALL_TARGET="bl1 bl2 bl31 fip"

python set_ddr_config(){
    if d.getVar("DDR_CONFIG"):
        d.setVar('DDR_PRIMARY_CONFIG','${DDR_CONFIG}')
        d.setVar('DDR_SECONDARY_CONFIG','${DDR_CONFIG}')
    
    d.appendVar('EXTRA_OEMAKE', ' DDR_PRIMARY_CONFIG=${DDR_PRIMARY_CONFIG}')
    d.appendVar('EXTRA_OEMAKE', ' DDR_SECONDARY_CONFIG=${DDR_SECONDARY_CONFIG}')
}

python enable_float_in_compile() {
    d.appendVar('EXTRA_OEMAKE', ' ENABLE_FP=1')
    #bb.warn("enabled FP: ",d.getVar("EXTRA_OEMAKE",True))
}

python disable_float_in_compile() {
    current = d.getVar("EXTRA_OEMAKE",True)
    d.setVar('EXTRA_OEMAKE', current.replace('ENABLE_FP=1',''))
    #bb.warn("disabled FP: ",d.getVar("EXTRA_OEMAKE",True))
}

python set_NON_FP_build_target() {
    new_build_target = d.getVar("NON_FP_BUILD_TARGET",True)
    d.setVar('TFA_BUILD_TARGET',new_build_target)
    #bb.warn("new TFA_BUILD_TARGET: ",d.getVar("TFA_BUILD_TARGET",True))
}

compile_build_target() {
    # Currently there are races if you build all the targets at once in parallel
    for T in ${TFA_BUILD_TARGET}; do
        oe_runmake -C ${S} $T
    done
}

python do_compile() {
    #Set DDR config variables for build
    bb.build.exec_func("set_ddr_config", d)

    #BL2 requires floating point to be enabled. TF-A can't set build flags conditionally per BL,
    #so a workaround is to build BL2 first with the flags we want, then build everything else second.
    bb.build.exec_func("enable_float_in_compile", d)
    bb.build.exec_func("compile_build_target", d)
    bb.build.exec_func("disable_float_in_compile", d)

    #build bl1 and others with FP disabled
    bb.build.exec_func("set_NON_FP_build_target", d)
    bb.build.exec_func("compile_build_target", d)
}

require ${MACHINE_TFA_REQUIRE}
