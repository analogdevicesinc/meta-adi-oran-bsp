DESCRIPTION = "ADI key for OP-TEE and TAs"
LICENSE = "CLOSED"
PROVIDES:append = " optee-keys-native"
RPROVIDES:${PN} = "optee-keys-native"

inherit native

SRC_URI = "file://keys"

do_install() {
    mkdir -p ${D}${datadir}/default-optee-keys
    install -D -p -m0755 ${WORKDIR}/keys/* ${D}${datadir}/default-optee-keys
}

FILES:${PN} = "${datadir}/default-optee-keys"