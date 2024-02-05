COMPATIBLE_MACHINE = "adrv904x-rd-ru"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/COPYING.MIT;md5=3da9cfbcb788c80a0384361b4de20420"

require checksums-donut.inc

RSU_FACT_APP1_URI = "file://files/${MACHINE}/${MACHINE}-refdesign.zip"
RSU_FACT_APP1_MD5 = "${FPGA_MD5}"
RSU_FACT_APP1_SH256 = "${FPGA_SH256}"

RSU_APP2_SOF_URI = "file://files/${MACHINE}/${MACHINE}-refdesign.zip"
RSU_APP2_SOF_MD5 = "${FPGA_MD5}"
RSU_APP2_SOF_SH256 = "${FPGA_SH256}"

require adrv904x-rd-ru-rsu.inc

require ../include/rsu-client-common.inc
