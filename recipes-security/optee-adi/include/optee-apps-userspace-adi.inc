SUMMARY = "OP-TEE apps"
DESCRIPTION = "ADI Open Portable Trusted Execution Environment -Applications"
LICENSE = "BSD-2-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=7f352a4cf2903f0e0f832383ace1f712"
PROVIDES:append = " optee-apps-userspace"
RPROVIDES:${PN} = "optee-apps-userspace"

DEPENDS = "optee-client optee-os-tadevkit python3-cryptography-native"

inherit python3native

require optee-uris-adi.inc
SRC_URI = "${OPTEE_APPS_URI};protocol=${OPTEE_APPS_PROT};branch=${OPTEE_APPS_BRANCH}"

COMPATIBLE_MACHINE = ""

S = "${WORKDIR}/git"
B = "${WORKDIR}/build"

CMAKE_INSTALL_BINDIR="${D}${bindir}"

inherit cmake

EXTRA_OECMAKE = ""

FILES:${PN} = "${bindir}"

# Imports machine specific configs from staging to build
PACKAGE_ARCH = "${MACHINE_ARCH}"
