DESCRIPTION = "Segmented JIC generation from FPGA image, based on which RSU client is built for ADI O-RAN platforms"
SECTION = "adi-rsu-client"

inherit deploy
require quartus_helpers.inc

#Below, the segmented JIC file for Qspi flash, and RSU client needs the FSBL.hex file generated at u-boot-socfpga build;
#and librsu built to system.
DEPENDS = "u-boot-socfpga intel-rsu dos2unix"
RDEPENDS:${PN}:append = " bash intel-rsu"
INHIBIT_DEFAULT_DEPS = "1"

FILESEXTRAPATHS:prepend := "${THISDIR}/..:"

CFG_FILE_WO_EXT ?= "initial_image_${MACHINE}"

SRC_URI = "file://files/${MACHINE}/${CFG_FILE_WO_EXT}.pfg"

SRC_URI:append = " ${RSU_FACT_APP1_URI};name=factory-app1-files;subdir=${BPN}-${PV}-factory-app1"
SRC_URI[factory-app1-files.md5sum] = "${RSU_FACT_APP1_MD5}"
SRC_URI[factory-app1-files.sh256sum] = "${RSU_FACT_APP1_SH256}"

SRC_URI:append = " ${RSU_APP2_PREV_URI};name=app2-prev;subdir=${BPN}-${PV}-app2-prev"
SRC_URI[app2-prev.md5sum] = "${RSU_APP2_PREV_MD5}"
SRC_URI[app2-prev.sh256sum] = "${RSU_APP2_PREV_SH256}"

SRC_URI:append = " ${RSU_APP2_SOF_URI};name=app2-sof;subdir=${BPN}-${PV}-app2"
SRC_URI[app2-sof.md5sum] = "${RSU_APP2_SOF_MD5}"
SRC_URI[app2-sof.sh256sum] = "${RSU_APP2_SOF_SH256}"

SRC_URI:append = " file://files/rsu_client_run.sh"

FACT-APP1-PATH = "${WORKDIR}/${BPN}-${PV}-factory-app1"
APP2-PREV-PATH = "${WORKDIR}/${BPN}-${PV}-app2-prev"
APP2-PATH      = "${WORKDIR}/${BPN}-${PV}-app2"

SEGMENTED_JIC_IMG = "initial_image_${MACHINE}.jic"
APP2_IMG          = "${SOC_FPGA_PARTNUM}.rpd"
APP2_PREV_IMG     = "${SOC_FPGA_PARTNUM}_test_image.rpd"
FACT_UPDATE_IMG   = "factory_image.rpd"
DEC_FW_IMG        = "decision_firmware_update.rpd"

do_compile() {
    load_quartus

    #Creating the Initial Flash Segmented JIC Image(combining Factory/Application0 Image + Application1 Image, in Section A)
    FACT_SOF=factory.sof #Initial flash image's factory design sof
    APP1_SOF=application1.sof
    cp ${FACT-APP1-PATH}/synth/output_files/${SOF_FILE}      ${S}/${FACT_SOF}
    cp ${FACT-APP1-PATH}/synth/output_files/${SOF_FILE}      ${S}/${APP1_SOF}
    cp ${TMPDIR}/deploy/images/${MACHINE}/u-boot-spl-dtb.hex ${S}/u-boot-spl-dtb.hex

    CFG_FILE=${CFG_FILE_WO_EXT}.pfg
    quartus_pfg -c ${WORKDIR}/files/${MACHINE}/${CFG_FILE}
 
    #application2_test_image.rpd is old(timestamp) build,accompanying for test purpose.
    cp ${APP2-PREV-PATH}/application2_prev.rpd ${S}/${APP2_PREV_IMG}

    #Creating the Application2 Image
    APP2_SOF=${SOC_FPGA_PARTNUM}.sof
    cp ${APP2-PATH}/synth/output_files/${SOF_FILE}      ${S}/${APP2_SOF}
    quartus_pfg -c ./${APP2_SOF} \
                   ./${APP2_IMG} \
                   -o hps_path=./u-boot-spl-dtb.hex \
                   -o mode=ASX4 \
                   -o start_address=0x00000 \
                   -o bitswap=ON

    #Creating the Factory Update Image
    FACT_UPDATE_SOF=factory_update.sof
    ##same as factory.sof
    cp ${FACT-APP1-PATH}/synth/output_files/${SOF_FILE} ${S}/${FACT_UPDATE_SOF}
    quartus_pfg -c ./${FACT_UPDATE_SOF} \
                   ./${FACT_UPDATE_IMG} \
                   -o hps_path=./u-boot-spl-dtb.hex \
                   -o mode=ASX4 \
                   -o start_address=0x00000 \
                   -o bitswap=ON \
                   -o rsu_upgrade=ON

    #Creating the Decision Firmware Update Image
    DEC_FW_SOF=factory_update.sof #same as factory update's sof
    ##same as factory.sof
    cp ${FACT-APP1-PATH}/synth/output_files/${SOF_FILE} ${S}/${DEC_FW_SOF}
    quartus_pfg -c ./${DEC_FW_SOF} \
                   ./${DEC_FW_IMG} \
                   -o hps_path=./u-boot-spl-dtb.hex \
                   -o mode=ASX4 \
                   -o start_address=0x00000 \
                   -o bitswap=ON \
                   -o rsu_upgrade=ON \
                   -o firmware_only=1
}

do_install() {
    install -d ${D}/opt/
    export PATH=/usr/bin:$PATH
    dos2unix       ${WORKDIR}/files/rsu_client_run.sh
    install -m 755 ${WORKDIR}/files/rsu_client_run.sh       ${D}/opt/rsu_client_run.sh
}

do_deploy() {
    install -d "${DEPLOYDIR}/rsu"
    install -m 0644 ${S}/${CFG_FILE_WO_EXT}.jic ${DEPLOYDIR}/rsu/${SEGMENTED_JIC_IMG}
    install -m 0644 ${S}/${APP2_IMG}            ${DEPLOYDIR}/rsu/${APP2_IMG}
    install -m 0644 ${S}/${APP2_PREV_IMG}       ${DEPLOYDIR}/rsu/${APP2_PREV_IMG}
    install -m 0644 ${S}/${FACT_UPDATE_IMG}     ${DEPLOYDIR}/rsu/${FACT_UPDATE_IMG}
    install -m 0644 ${S}/${DEC_FW_IMG}          ${DEPLOYDIR}/rsu/${DEC_FW_IMG}
    install -m 0644 ${WORKDIR}/files/rsu_client_run.sh         ${DEPLOYDIR}/rsu/rsu_client_run.sh 
}

addtask do_deploy after do_install before do_build

PROVIDES += "adi-rsu-client"

PACKAGE_ARCH = "${MACHINE_ARCH}"

#shipped the installed files to the rootfs:
FILES:${PN} += " /opt/rsu_client_run.sh "
