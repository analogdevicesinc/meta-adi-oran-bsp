FILESEXTRAPATHS:prepend := "${THISDIR}:"

SRC_URI:append:adrv904x-rd-ru = " \
    file://files/socfpga_adrv904x-rd-ru_socdk.dts \
    file://files/socfpga_adrv904x-rd-ru.dtsi \
    file://config/adrv904x-rd-ru.cfg \
    file://files/Documentation/devicetree/bindings/ptp/ptp-adi.yaml \
    file://files/Documentation/devicetree/bindings/clock/clk-ad9545.yaml \
    file://files/drivers/clk/adi/clk-ad9545.c \
    file://files/drivers/clk/adi/clk-ad9545.h \
    file://files/drivers/clk/adi/clk-ad9545-i2c.c \
    file://files/drivers/clk/adi/clk-ad9545-spi.c \
    file://files/drivers/clk/adi/Kconfig \
    file://files/drivers/clk/adi/Makefile \
    file://files/drivers/net/ethernet/adi-msp.c \
    file://files/drivers/ptp/adi_ptp/ptp_adi.c \
    file://files/drivers/ptp/adi_ptp/ptp_adi.h \
    file://files/drivers/ptp/adi_ptp/ptp_adi_clk.c \
    file://files/drivers/ptp/adi_ptp/ptp_adi_clk.h \
    file://files/drivers/ptp/adi_ptp/Makefile \
    file://files/include/dt-bindings/clock/ad9545.h \
    file://files/include/linux/adi_phc.h \
    file://files/include/linux/clk/ad9545.h \
    file://files/adrv904x-rd-ru_blacklist.conf \
    "

do_patch:append () {
    machine_upper="$(echo ${MACHINE} | tr [:lower:] [:upper:])"
    mkdir -p ${S}/arch/arm64/boot/dts/adi/

    echo "
    # SPDX-License-Identifier: GPL-2.0-only
    dtb-\$(CONFIG_ARCH_${machine_upper}) += socfpga_${MACHINE}_socdk.dtb
    " > ${S}/arch/arm64/boot/dts/adi/Makefile

    # Ideal implementation but ^^ isn't supported by Yocto.
    #echo "
    ## SPDX-License-Identifier: GPL-2.0-only
    #dtb-\$(CONFIG_ARCH_${MACHINE^^}) += socfpga_${MACHINE}_socdk.dtb
    #" > ${S}/arch/arm64/boot/dts/adi/Makefile

    cp ${WORKDIR}/files/socfpga_${MACHINE}_socdk.dts ${S}/arch/arm64/boot/dts/adi/
    cp ${WORKDIR}/files/socfpga_${MACHINE}.dtsi ${S}/arch/arm64/boot/dts/adi/
    echo "subdir-y += adi\n" >> ${S}/arch/arm64/boot/dts/Makefile
}

do_patch:append:adrv904x-rd-ru () {
    # copy over source files/docs of adi-ptp/ad9545/adi-msp drivers
    cp -rf ${WORKDIR}/files/drivers/        ${S}/
    cp -rf ${WORKDIR}/files/include/        ${S}/
    cp -rf ${WORKDIR}/files/Documentation/  ${S}/
}

BLACKLIST_CONF = ""
BLACKLIST_CONF:adrv904x-rd-ru = "files/adrv904x-rd-ru_blacklist.conf"
include ${BLACKLIST_CONF}
