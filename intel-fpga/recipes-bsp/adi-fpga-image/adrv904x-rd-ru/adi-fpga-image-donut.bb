COMPATIBLE_MACHINE = "adrv904x-rd-ru"

PROVIDES:append = " adi-fpga-image"
RPROVIDES:${PN} = "adi-fpga-image"

require checksums-donut.inc
require ../include/donut-fpga-image.inc
