# meta-adi-oran-bsp

This Yocto layer defines a board support package for all of ADI's Yocto-based
O-RAN transceiver reference hardware designs. This includes firmware, the boot
chain, a kernel, and drivers, but does not include the userspace software.
(Software is provided in another layer.)

## Machines

This layer defines the following machines:

* adrv904x-rd-ru (a.k.a. Kerberos) is the O-RAN mid-band radio unit reference
  design for adrv904x-class transceivers.

## Dependencies

oe-core (kirkstone)
meta-arm

For FPGA platforms:
meta-intel-fpga

## For Intel FPGA platforms

Download & install the appropriate version of Quartus Programmer tool. Use the
following install path: `/opt/intelFPGA_pro/<version>`.
(You can download the programmer only from intel.com, e.g. v21.3 installer:
 QuartusProProgrammerSetup-21.3.0.170-linux.run)

* **21.3**: adrv904x-rd-ru

Furthermore, please ensure that libncurses5 is installed:
`sudo apt-get install libncurses5`

## Licensing

Recipes are released under the MIT license. See `COPYING.MIT` for details.

## Patches

Please submit any patches against the meta-adi-oran-sw layer to the O-RAN
Support mailing list (pci_support@analog.com).

## Building for adrv904x-rd-ru

Configure Yocto to build for adrv904x-rd-ru:

0. `machine=adrv904x-rd-ru`
1. `echo "" >> conf/local.conf`
2. `echo "MACHINE = \"${machine}\"" >> conf/local.conf`
3. `echo "DISTRO = \"oran-distro\"" >> conf/local.conf`

To build U-Boot, run:               `bitbake virtual/bootloader` \
To build the kernel, run:           `bitbake virtual/kernel` \
To build the root filesystem, run:  `bitbake adi-console-image -c image_cpio` \
To build SD image, run:             `bitbake adi-console-image`

NOTE:
Version strings for Linux and U-Boot to include ADI platform names set in steps
6 & 7 are optional.

## System and image features

These images contain features can be enabled/disabled, such as INTEL-FPGA*,TF-A
or Intel Remote System Update (RSU). This section will list features and 
document how they're enabled.

> Note: `INTEL_FPGA` is enabled by default for machines with Intel FPGAs, like
> the Agilex on adrv904x-rd-ru. It is disabled by default on machines that do
> not have an FPGA.

### Enable TF-A

**Description**: Enable or disable Arm trusted firmware in the boot image. This
feature is  enabled by default, to disable, set `ADI_CC_TFA="0"` in local.conf.

**Relevant Variables**: `ADI_CC_TFA`

**Default Values**: `ADI_CC_TFA="1"`

### Enable Intel RSU

**Description**: Enable or disable Intel Remote Update in the system image.
This feature is enabled by default, to disable, set `ADI_CC_RSU="0"` in
local.conf.

**Relevant Variables**: `ADI_CC_RSU`

**Default Values**: `ADI_CC_RSU="1"`

### Login with SSH key

**Description**: Configure SSH public key in the system image to allow ssh
login. This feature is disabled by default, to enable, set
`ADI_CC_SSH_PUBKEY` to the ssh public key to use in in local.conf.

**Relevant Variables**: `ADI_CC_SSH_KEY_DIR`

**Default Values**: `ADI_CC_SSH_KEY_DIR=""`

### SD image size

> Note: valid only for adrv904x-rd-ru

Set the SD image size by setting bootsize and rootfssize respectively, e.g.:

1. `echo "BOOT_SIZE = \"256M\"" >> conf/local.conf`
2. `echo "ROOTFS_SIZE = \"1024M\"" >> conf/local.conf`
