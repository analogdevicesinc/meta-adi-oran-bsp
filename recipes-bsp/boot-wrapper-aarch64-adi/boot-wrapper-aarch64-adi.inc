# ADI aarch64 boot wrapper recipe based on:
# yocto/meta-arm/meta-arm/recipes-bsp/boot-wrapper-aarch64/boot-wrapper-aarch64_git.bb

SUMMARY = "ADI Linux aarch64 boot wrapper with FDT support"
LICENSE = "BSD-3-Clause"

PROVIDES:append = " boot-wrapper-aarch64"

SRC_URI = "git://git@github.com/analogdevicesinc/oran-bootwrapper-aarch64.git;protocol=ssh;branch=main"

S = "${WORKDIR}/git"

inherit autotools deploy

PACKAGE_ARCH = "${MACHINE_ARCH}"
COMPATIBLE_MACHINE = "titan-*"
DEPENDS:append = " virtual/kernel dtc-native"

EXTRA_OECONF:append = " CFLAGS='-g -O0'"
EXTRA_OECONF:append = " --enable-gicv3"
EXTRA_OECONF:append = " --with-dtb=${STAGING_DIR_HOST}/boot/fdt/adrv906x-secondary.dtb"
EXTRA_OECONF:append = " --enable-gic600 --enable-adi-platform --enable-dynamic-config"

# Unset LDFLAGS solves this error when compiling kernel modules:
# aarch64-poky-linux-ld: unrecognized option '-Wl,-O1'
EXTRA_OEMAKE:append = " LDFLAGS='--gc-sections'"

do_configure[depends] += "virtual/kernel:do_populate_sysroot"

do_configure:prepend() {
    (cd ${S} && autoreconf -i || exit 1)
}

do_install(){
    install -d ${D}/boot-wrapper-aarch64/include
    install -m 644 ${S}/include/*  ${D}/boot-wrapper-aarch64/include
}

do_deploy(){
    ${OBJCOPY} -O binary ${B}/linux-system.axf ${DEPLOYDIR}/boot-wrapper.bin
}

rm_files_after_sharing(){
    #we share the header files to secondary-app-pack, but don't want to ship it to image(avoid potentially being built to console-image).
    rm -rf ${D}/boot-wrapper-aarch64
}

do_populate_sysroot:append(){
    bb.build.exec_func('rm_files_after_sharing', d)
}

addtask deploy before do_build after do_compile
addtask package before do_packagedata after do_populate_sysroot

#for usage by secondary-app-pack building
SYSROOT_DIRS:append = " /boot-wrapper-aarch64/include"
