DESCRIPTION = "FPGA images for ADI O-RAN platforms"
SECTION = "adi-fpga-image"

inherit deploy
require quartus_helpers.inc

#Below, the FPGA image creation(FSBL/JIC image) needs the FSBL.hex file generated at u-boot-socfpga build.
DEPENDS = "u-boot-socfpga"
INHIBIT_DEFAULT_DEPS = "1"

FILESEXTRAPATHS:prepend := "${THISDIR}/..:"

SRC_URI = "${FPGA_URI};name=fpga-files;subdir=${BPN}-${PV}-${MACHINE}"
SRC_URI[fpga-files.md5sum] = "${FPGA_MD5}"
SRC_URI[fpga-files.sha256sum] = "${FPGA_SH256}"

SRC_URI:append = " file://files/${MACHINE}/gen_${MACHINE}_hps_core_rbf.pfg"
SRC_URI:append = " file://files/${MACHINE}/gen_${MACHINE}_jic.pfg"

S = "${WORKDIR}/${BPN}-${PV}-${MACHINE}"

do_compile() {
    load_quartus

    quartus_pfg -c ${S}/synth/output_files/${SOF_FILE} ${SOC_FPGA_PARTNUM}_hps.sof -o hps_path=${DEPLOY_DIR_IMAGE}/u-boot-spl-dtb.hex
    quartus_pfg -c ${WORKDIR}/files/${MACHINE}/gen_${MACHINE}_hps_core_rbf.pfg
    quartus_pfg -c ${WORKDIR}/files/${MACHINE}/gen_${MACHINE}_jic.pfg
}

do_deploy() {
    install -m 0644 ${S}/${SOC_FPGA_PARTNUM}_hps.jic ${DEPLOYDIR}/${SOC_FPGA_PARTNUM}_hps.jic-${MACHINE}
    ln -s ${SOC_FPGA_PARTNUM}_hps.jic-${MACHINE} ${DEPLOYDIR}/${SOC_FPGA_PARTNUM}_hps.jic

    install -m 0644 ${S}/${SOC_FPGA_PARTNUM}.core.rbf ${DEPLOYDIR}/${SOC_FPGA_PARTNUM}.core.rbf-${MACHINE}
    ln -s ${SOC_FPGA_PARTNUM}.core.rbf-${MACHINE} ${DEPLOYDIR}/${SOC_FPGA_PARTNUM}.core.rbf
}

addtask do_deploy after do_install before do_build

PACKAGE_ARCH = "${MACHINE_ARCH}"
