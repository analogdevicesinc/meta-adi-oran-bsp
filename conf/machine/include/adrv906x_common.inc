#@TYPE: Machine
#@NAME: Titan
#@DESCRIPTION: Machine configuration for the ADI Samana evaluation platform

EXTRA_IMAGEDEPENDS:append = " trusted-firmware-a-adi app-pack debug-app-pack"

DEFAULTTUNE = "cortexa55"
#include conf/machine/include/arm/arch-arm64.inc
require conf/machine/include/arm/armv8-2a/tune-cortexa55.inc

SOC ?= "samana"

ADI_SOC ?= "adrv906x"

# default DDR configuration
DDR_CONFIG ?= "DDR_2GB_1rank_x16_1gbx16_3200"

DISTRO_VERSION = "${@d.getVar('VERSION_META').split('-')[0]}"
ANTI_ROLLBACK_VERSION = "0"

SERIAL_CONSOLES ?= "115200;ttyAMA0"
# Must be empty to support read-only rootfs
SERIAL_CONSOLES_CHECK = ""

KERNEL_IMAGETYPE = "Image.gz"
KERNEL_DEVICETREE = "adi/adrv906x-${MACHINE}.dtb adi/adrv906x-secondary.dtb"
KBUILD_DEFCONFIG = "adrv906x-eval_defconfig"

KERNEL_CLASSES:append = " kernel-fitimage-adi"
KERNEL_IMAGETYPES:append = " fitImage"

INITRAMFS_IMAGE = "adrv906x-base-initramfs"
INITRAMFS_IMAGE_BUNDLE = "0"

TFA_PLATFORM = "${@d.getVar('MACHINE', True).replace('-','_')}"
TFA_HW_CONFIG = "${TFA_PLATFORM}"
TFA_FW_CONFIG = "${TFA_PLATFORM}-fw-config"

UBOOT_MACHINE = "adrv906x-eval_defconfig"
UBOOT_SIGN_ENABLE = "1"
UBOOT_SIGN_KEYDIR = "${TMPDIR}/work-shared/${MACHINE}/fit-keys"
UBOOT_SIGN_KEYNAME = "dev"
# UBOOT_MKIMAGE_DTCOPTS = "-I dts -O dtb -p 2000"
UBOOT_DTB_BINARY = "${TFA_HW_CONFIG}.dtb"

UBOOT_ENTRYPOINT = "0x46000000"
TFA_PRI_BL1_LOAD_ADDR = "0x005DF000"
TFA_SEC_BL1_LOAD_ADDR = "0x045E0000"

KERNEL_FIT_IMAGE ?= "kernel_fit.itb"
IMAGE_BOOT_FILES ?= "${KERNEL_FIT_IMAGE}"
WKS_FILE ?= "adrv906x-sd.wks.in"

IMAGE_CLASSES += "dm-verity-img-adi"
DM_VERITY_IMAGE = "adrv906x-base-image"
DM_VERITY_IMAGE_TYPE = "ext4"
DM_VERITY_IMAGE_DATA_BLOCK_SIZE = "4096"
DM_VERITY_IMAGE_HASH_BLOCK_SIZE = "4096"
DM_VERITY_IMAGE_FEC_ROOTS = "2"

USE_VT ?= "0"

# Flag to build custom BL1 and app-pack for secure debug access and RMA
RMA_CLI = "0"

# =============================================================================
# NFS boot support
# =============================================================================
# Default IP addresses, override in local.conf
samana_ipaddr ?="192.168.0.2"
nfs_server_ipaddr ?="192.168.1.20"

# Default nfs dir. override in local.conf
nfs_dir ?= "${DEPLOY_DIR_IMAGE}/nfs"

# nfs_boot_enable, override in local.conf
nfs_boot_enable ?= "0"

require adrv906x_nor_flash_layout.inc
