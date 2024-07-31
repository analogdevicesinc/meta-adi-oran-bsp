FILESEXTRAPATHS:prepend := "${THISDIR}:"

SRC_URI:append:adrv904x-rd-ru = " \
    file://files/socfpga_adrv904x-rd-ru_socdk.dts \
    file://files/socfpga_adrv904x-rd-ru.dtsi \
    file://config/adrv904x-rd-ru.cfg \
    file://config/socfpga_adrv904x-rd-ru_defconfig \
    file://files/Documentation/devicetree/bindings/ptp/ptp-adi.yaml \
    file://files/Documentation/devicetree/bindings/clock/clk-ad9545.yaml \
    file://files/drivers/Kconfig \
    file://files/drivers/Makefile \
    file://files/drivers/base/dd.c \
    file://files/drivers/clk/Kconfig \
    file://files/drivers/clk/Makefile \
    file://files/drivers/clk/clk.c \
    file://files/drivers/clk/adi/clk-ad9545.c \
    file://files/drivers/clk/adi/clk-ad9545.h \
    file://files/drivers/clk/adi/clk-ad9545-i2c.c \
    file://files/drivers/clk/adi/clk-ad9545-spi.c \
    file://files/drivers/clk/adi/Kconfig \
    file://files/drivers/clk/adi/Makefile \
    file://files/drivers/gpio/Kconfig \
    file://files/drivers/gpio/Makefile \
    file://files/drivers/gpio/gpio-kerberos-qsfp.c \
    file://files/drivers/iio/frequency/Kconfig \
    file://files/drivers/iio/frequency/Makefile \
    file://files/drivers/iio/frequency/ad9528.c \
    file://files/drivers/jesd204/Kconfig \
    file://files/drivers/jesd204/Makefile \
    file://files/drivers/jesd204/attr.h \
    file://files/drivers/jesd204/jesd204-core.c \
    file://files/drivers/jesd204/jesd204-fsm.c \
    file://files/drivers/jesd204/jesd204-priv.h \
    file://files/drivers/jesd204/jesd204-sysfs.c \
    file://files/drivers/jesd204/jesd204_top_device.c \
    file://files/drivers/net/ethernet/adi-msp.c \
    file://files/drivers/net/ethernet/Kconfig \
    file://files/drivers/net/ethernet/Makefile \
    file://files/drivers/net/ethernet/altera/Kconfig \
    file://files/drivers/net/ethernet/altera/Makefile \
    file://files/drivers/net/ethernet/altera/altera_utils.h \
    file://files/drivers/net/ethernet/altera/intel_fpga_etile_ethtool.c \
    file://files/drivers/net/ethernet/altera/intel_fpga_etile.h \
    file://files/drivers/net/ethernet/altera/intel_fpga_etile_main.c \
    file://files/drivers/net/phy/Kconfig \
    file://files/drivers/net/phy/qsfp.c \
    file://files/drivers/net/phy/sfp.c \
    file://files/drivers/ptp/Kconfig \
    file://files/drivers/ptp/Makefile \
    file://files/drivers/ptp/adi_ptp/ptp_adi.c \
    file://files/drivers/ptp/adi_ptp/ptp_adi.h \
    file://files/drivers/ptp/adi_ptp/ptp_adi_clk.c \
    file://files/drivers/ptp/adi_ptp/ptp_adi_clk.h \
    file://files/drivers/ptp/adi_ptp/Makefile \
    file://files/drivers/spi/Kconfig \
    file://files/drivers/spi/Makefile \
    file://files/drivers/spi/spi-axi-adv-spi.c \
    file://files/include/dt-bindings/clock/ad9545.h \
    file://files/include/dt-bindings/jesd204/adxcvr.h \
    file://files/include/dt-bindings/jesd204/device-states.h \
    file://files/include/dt-bindings/iio/frequency/ad9528.h \
    file://files/include/linux/adi_phc.h \
    file://files/include/linux/clk-provider.h \
    file://files/include/linux/clk.h \
    file://files/include/linux/clk/ad9545.h \
    file://files/include/linux/device/driver.h \
    file://files/include/linux/iio/frequency/ad9528.h \
    file://files/include/linux/jesd204/adi-common.h \
    file://files/include/linux/jesd204/jesd204-of.h \
    file://files/include/linux/jesd204/jesd204.h \
    file://files/include/linux/math64.h \
    file://files/include/trace/events/clk.h \
    file://files/adrv904x-rd-ru_blacklist.conf \
    "

do_validate_branches:append () {
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

    if ${@bb.utils.contains('PREFERRED_PROVIDER_adi-fpga-image', 'adi-fpga-image-oran-10g', 'true', 'false', d)}; then
       sed -i "s/390625000/156250000/g" ${WORKDIR}/files/socfpga_${MACHINE}_socdk.dts
    fi

    cp ${WORKDIR}/files/socfpga_${MACHINE}_socdk.dts ${S}/arch/arm64/boot/dts/adi/
    cp ${WORKDIR}/files/socfpga_${MACHINE}.dtsi ${S}/arch/arm64/boot/dts/adi/
    echo "subdir-y += adi\n" >> ${S}/arch/arm64/boot/dts/Makefile
}

do_validate_branches:append:adrv904x-rd-ru () {
    cp ${WORKDIR}/config/socfpga_${MACHINE}_defconfig ${WORKDIR}/defconfig

    # copy over source files/docs of adi-ptp/ad9545/adi-msp drivers
    cp -rf ${WORKDIR}/files/drivers/        ${S}/
    cp -rf ${WORKDIR}/files/include/        ${S}/
    cp -rf ${WORKDIR}/files/Documentation/  ${S}/
}

BLACKLIST_CONF = ""
BLACKLIST_CONF:adrv904x-rd-ru = "files/adrv904x-rd-ru_blacklist.conf"
include ${BLACKLIST_CONF}
