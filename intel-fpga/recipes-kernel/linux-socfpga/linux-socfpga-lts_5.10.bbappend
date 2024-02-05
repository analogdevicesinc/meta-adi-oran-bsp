# for Agilex, Intel only officially support 5.10.100 -- but the RSU driver was added in 5.10.110
# so we only overwrite the defaults if the RSU feature is enabled.

LINUX_VERSION =  "${@bb.utils.contains('ADI_CC_RSU', '1', '5.10.110', '5.10.100', d)}"
SRCREV = "${@bb.utils.contains('ADI_CC_RSU', '1', '0d88e76816a99decc351739974918134966fc930', '47109d445dc0f922f6612ed1f5a229ea3f62f97e', d)}"

#different patch files for 5.10.100/110
SRC_URI:append:adrv904x-rd-ru = "${@bb.utils.contains('LINUX_VERSION', '5.10.100', ' file://files/adrv904x-rd-ru_drivers-5.10.100.patch', ' ', d)}"
SRC_URI:append:adrv904x-rd-ru = "${@bb.utils.contains('LINUX_VERSION', '5.10.110', ' file://files/adrv904x-rd-ru_drivers-5.10.110.patch', ' ', d)}"
