KERNEL_REPO = "git://git@github.com/analogdevicesinc/oran-linux.git"
KERNEL_PROT = "ssh"
KBRANCH = "main"

SRC_URI = "${KERNEL_REPO};protocol=${KERNEL_PROT};branch=${KBRANCH}"
SRC_URI:append = "${@bb.utils.contains('ADI_CC_BOOT_DEBUG','1' ,' file://linux-adi-debug.cfg', ' ', d)}"
