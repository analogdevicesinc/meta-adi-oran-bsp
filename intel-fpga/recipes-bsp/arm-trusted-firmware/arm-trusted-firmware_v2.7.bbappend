COMPATIBLE_MACHINE = "adrv904x-rd-ru"
ATFPLAT:adrv904x-rd-ru = "agilex"

do_compile:prepend(){
    sed -i 's/BL31:/BL31-${MACHINE}:/g'  ${S}/bl31/bl31_main.c
    sed -i 's/CRASH_CONSOLE_BASE.PLAT_UART0_BASE/CRASH_CONSOLE_BASE    PLAT_UART1_BASE/g' \
        ${S}/plat/intel/soc/common/include/platform_def.h
    sed -i 's/PLAT_INTEL_UART_BASE.PLAT_UART0_BASE/PLAT_INTEL_UART_BASE    PLAT_UART1_BASE/g' \
        ${S}/plat/intel/soc/common/include/platform_def.h
}