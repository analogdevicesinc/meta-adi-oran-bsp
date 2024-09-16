# optee.inc sets OPTEEMACHINE ?= "${MACHINE}"
# optee-os.inc adds PLATFORM=${OPTEEMACHINE} to EXTRA_OEMAKE
# We set OPTEEMACHINE here, so the PLATFORM is correctly defined.
OPTEEMACHINE = "adi"
PLATFORM_FLAVOR = "${ADI_SOC}_${OPTEE_PLATFORM}"

ADI_CC_BOOT_DEBUG ?= "0"
