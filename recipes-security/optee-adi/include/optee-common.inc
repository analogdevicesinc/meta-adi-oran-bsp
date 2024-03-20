# optee.inc sets OPTEEMACHINE ?= "${MACHINE}"
# optee-os.inc adds PLATFORM=${OPTEEMACHINE} to EXTRA_OEMAKE
# We set OPTEEMACHINE here, so the PLATFORM is correctly defined.
OPTEEMACHINE = "adi"
PLATFORM_FLAVOR:titan-4 = "adrv906x_eval"
PLATFORM_FLAVOR:titan-8 = "adrv906x_eval"

ADI_CC_BOOT_DEBUG ?= "0"
