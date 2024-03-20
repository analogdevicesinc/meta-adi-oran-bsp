SUMMARY = "OP-TEE apps"
DESCRIPTION = "ADI Open Portable Trusted Execution Environment -Applications"
LICENSE = "BSD-2-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=7f352a4cf2903f0e0f832383ace1f712"
PROVIDES:append = " optee-apps"
RPROVIDES:${PN} = "optee-apps"

DEPENDS = "optee-client optee-os-tadevkit python3-cryptography-native"

inherit python3native

require optee-common.inc
require recipes-security/optee/optee.inc

require optee-uris-adi.inc
SRC_URI = "${OPTEE_APPS_URI};protocol=${OPTEE_APPS_PROT};branch=${OPTEE_APPS_BRANCH}"

COMPATIBLE_MACHINE = ""

EXTRA_OEMAKE += "TA_DEV_KIT_DIR=${TA_DEV_KIT_DIR} \
                 CROSS_COMPILE=${HOST_PREFIX} \
                "
                
S = "${WORKDIR}/git"
B = "${WORKDIR}/build"

DEPENDS += " optee-os-tadevkit "
do_compile[depends] += "optee-os-tadevkit:do_deploy"

# Default Keys
DEPENDS += " optee-keys-native"
DEFAULT_TA_SIGNING_PRIV_KEY_PATH = "${RECIPE_SYSROOT_NATIVE}${datadir}/default-optee-keys/optee_priv_key.pem"
DEFAULT_TA_SIGNING_PUB_KEY_PATH = "${RECIPE_SYSROOT_NATIVE}${datadir}/default-optee-keys/optee_pub_key.pem"
# Used Keys
TA_SIGNING_PRIV_KEY_PATH ??= "${DEFAULT_TA_SIGNING_PRIV_KEY_PATH}"
TA_SIGNING_PUB_KEY_PATH ??= "${DEFAULT_TA_SIGNING_PUB_KEY_PATH}"

# We need base64 for signing:
DEPENDS += " coreutils-native "
do_compile() {
    # Early-TAs
    EARLY_TAS=$(find ${S} -type d -name "early_ta")
    for ta in ${EARLY_TAS};do
        # Strip ${S} dir name from the found TA
        ta=${ta#${S}/}
        oe_runmake O="${B}/$ta" -C ${S}/$ta all
    done
    # TAs
    TAS=$(find ${S} -type d -name "ta")
    for ta in ${TAS};do
        # Strip ${S} dir name from the found TA
        ta=${ta#${S}/}
        oe_runmake O="${B}/$ta" -C ${S}/$ta all
    done
}

do_re_sign_TAs() {
    # TAs
    TAS=$(find ${S} -type d -name "ta")
    for ta in ${TAS};do
        # Strip ${S} dir name from the found TA
        ta=${ta#${S}/}

        cd ${B}/$ta

        ta_name=$(basename $(ls *.ta))
        ta_uuid=${ta_name%.*}

        # Rename the old TA (signed with the optee_os dummy key)
        mv ${ta_uuid}.ta ${ta_uuid}.ta.old_sign

        ## Sign TAs (https://optee.readthedocs.io/en/latest/building/trusted_applications.html#signing-of-tas) ##
        # Manually generate a digest of the generated .stripped.elf files
        ${PYTHON3} ${TA_DEV_KIT_DIR}/scripts/sign_encrypt.py digest \
            --key  ${TA_SIGNING_PRIV_KEY_PATH} \
            --uuid ${ta_uuid} \
            --in   ${ta_uuid}.stripped.elf \
            --dig  ${ta_uuid}.dig

        # Sign this digest offline
        base64 --decode ${ta_uuid}.dig \
            | openssl pkeyutl -sign \
                -inkey ${TA_SIGNING_PRIV_KEY_PATH} \
                -pkeyopt digest:sha256 \
                -pkeyopt rsa_padding_mode:pss \
                -pkeyopt rsa_pss_saltlen:digest \
                -pkeyopt rsa_mgf1_md:sha256 \
            | base64 > ${ta_uuid}.sig

        # Manually stitch the TA and signature together
        ${PYTHON3} ${TA_DEV_KIT_DIR}/scripts/sign_encrypt.py stitch \
            --key  ${TA_SIGNING_PUB_KEY_PATH} \
            --uuid ${ta_uuid} \
            --in   ${ta_uuid}.stripped.elf \
            --sig  ${ta_uuid}.sig \
            --out  ${ta_uuid}.ta
    done
}
addtask re_sign_TAs after do_compile before do_install

# Early TAs deploy dir (must match the one defined in our optee-os_3.%.bbappend)
ADI_EARLY_TAS_DEPLOY_DIR = "${STAGING_DIR}/optee-apps-early-tas"

# Install elfs in staging area to be used by the optee os build
do_install(){
    # Early-TAs
    mkdir -p ${ADI_EARLY_TAS_DEPLOY_DIR}
    EARLY_TAS=$(find ${B} -type d -name "early_ta")
    for ta in ${EARLY_TAS};do
        #strip ${B} dir name from the found TA
        ta=${ta#${B}/}
        install -m0644 ${B}/$ta/*.stripped.elf ${ADI_EARLY_TAS_DEPLOY_DIR}
    done
    # TAs
    TAS=$(find ${B} -type d -name "ta")
    for ta in ${TAS};do
        #strip ${B} dir name from the found TA
        ta=${ta#${B}/}
        mkdir -p ${D}${nonarch_base_libdir}/optee_armtz/
        install -D -p -m0444 ${B}/$ta/*.ta ${D}${nonarch_base_libdir}/optee_armtz/
        #mkdir -p ${D}${libdir}/tee-supplicant/plugins
        #install -D -p -m0444 ${B}/$ta/*.plugin ${D}${libdir}/tee-supplicant/plugins/
    done
}

FILES:${PN} = "${ADI_EARLY_TAS_DEPLOY_DIR} \
               ${nonarch_base_libdir}/optee_armtz \
               ${libdir}/tee-supplicant/plugins "

do_compile[cleandirs] = "${B}"

# Imports machine specific configs from staging to build
PACKAGE_ARCH = "${MACHINE_ARCH}"
