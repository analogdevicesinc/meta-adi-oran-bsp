PROVIDES:append = " boot-wrapper-aarch64"

inherit autotools deploy

PACKAGE_ARCH = "${MACHINE_ARCH}"
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

do_deploy(){
    ${OBJCOPY} -O binary ${B}/linux-system.axf ${DEPLOYDIR}/boot-wrapper.bin
}

addtask deploy before do_build after do_compile
addtask package before do_packagedata after do_populate_sysroot
