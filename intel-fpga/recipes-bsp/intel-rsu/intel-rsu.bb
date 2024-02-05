DESCRIPTION = "Agilex SocFPGA Remote System Update(RSU) library for ADI O-RAN platforms"
SECTION = "intel-rsu"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/COPYING.MIT;md5=3da9cfbcb788c80a0384361b4de20420"

# Machines supporting Intel RSU
COMPATIBLE_MACHINE = "adrv904x-rd-ru"

#cross-compiler setup
DEPENDS += "virtual/${TARGET_PREFIX}gcc libgcc virtual/${MLPREFIX}libc "
DEPENDS += " zlib "
RDEPENDS:${PN}+= " libgcc "

SRCBRANCH = "master"
SRC_URI   = "git://github.com/altera-opensource/intel-rsu;protocol=https;branch=${SRCBRANCH}"
SRCREV    = "4018a2487db84ae91f72d5eafcd10c279e7bd72a"

S = "${WORKDIR}/git"

EXTRA_OEMAKE = "CROSS_COMPILE=${TARGET_PREFIX} \
 CFLAGS=' -nostdlib -I../include -I${RECIPE_SYSROOT}/usr/include -I${RECIPE_SYSROOT}/usr/lib -I${STAGING_LIBDIR}/zlib \
 -fPIC -fPIE -Wall -Wsign-compare -Wpedantic -Wno-variadic-macros -Wfatal-errors \
 -fstack-protector-strong \
 -O2 -D_FORTIFY_SOURCE=2 \
 -Wformat -Wformat-security \
'"

do_compile_rsu_client() {
    oe_runmake CROSS_COMPILE=${TARGET_PREFIX} CFLAGS=" -nostdlib -I../include -I${RECIPE_SYSROOT}/usr/include  -I${RECIPE_SYSROOT}/usr/lib -I${STAGING_LIBDIR}/zlib  -Wall -Wsign-compare -Wpedantic -Werror -Wno-variadic-macros -Wfatal-errors " LDFLAGS=" --sysroot=${RECIPE_SYSROOT} -L../lib/ -lrsu -lz -L${STAGING_LIBDIR}/zlib -L${RECIPE_SYSROOT}/usr/lib " -C ${S}/example
}

do_compile() {
    oe_runmake LDFLAGS="-shared -z noexecstack -z relro -z now  --sysroot=${RECIPE_SYSROOT} -L${RECIPE_SYSROOT}/usr/lib -L${RECIPE_SYSROOT}/lib -L${STAGING_LIBDIR}/zlib " -C ${S}/lib

    do_compile_rsu_client
}

do_install() {
    # create the /lib folder in the rootfs with default permissions
    install -d ${D}${libdir}
    install -d ${D}${bindir}
    install -d ${D}/etc/

    # install the library into the /usr/lib folder with default permissions
    install -m 755 ${S}/lib/librsu.so      ${D}${libdir}
    install -m 755 ${S}/example/rsu_client ${D}${bindir}
    install -m 755 ${S}/etc/qspi.rc        ${D}/etc/librsu.rc
}

PROVIDES += "intel-rsu"

PACKAGE_ARCH = "${MACHINE_ARCH}"

#shipped the installed files to the rootfs:
FILES:${PN} += " ${libdir}/* /etc/librsu.rc ${bindir}/rsu_client "
INSANE_SKIP:${PN} = "ldflags"
#staging the *.so library image
INHIBIT_PACKAGE_STRIP = "1"
INHIBIT_SYSROOT_STRIP = "1"
SOLIBS = ".so"
FILES_SOLIBSDEV = ""

