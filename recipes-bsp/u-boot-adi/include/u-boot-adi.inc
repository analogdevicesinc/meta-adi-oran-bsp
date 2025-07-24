SRC_URI = "git://git@github.com/analogdevicesinc/oran-u-boot.git;protocol=ssh;branch=main"
SRC_URI:append = "${@'' if '-N pkcs11' in d.getVar('UBOOT_MKIMAGE_SIGN_ARGS', True) else ' file://fit_keys/dev.crt file://fit_keys/dev.key'}"
SRC_URI:append = "${@bb.utils.contains('ADI_CC_BOOT_DEBUG','1' ,' file://u-boot-adi-debug.cfg', ' ', d)}"
S = "${WORKDIR}/git"

def get_openssl_engines(d):
    args = d.getVar('UBOOT_MKIMAGE_SIGN_ARGS', expand=True)
    val = ''
    if '-N pkcs11' in args:
        val += d.getVar('RECIPE_SYSROOT_NATIVE') + '/usr/lib/engines-3/'
    return val

def get_pkcs11_module(d):
    args = d.getVar('UBOOT_MKIMAGE_SIGN_ARGS', expand=True)
    val = ''
    if '-N pkcs11' in args:
        val += d.getVar('RECIPE_SYSROOT_NATIVE') + '/usr/lib/pkcs11/p11-kit-client.so'
    return val

def get_extra_depends(d):
    args = d.getVar('UBOOT_MKIMAGE_SIGN_ARGS', expand=True)
    val = ''
    if '-N pkcs11' in args:
        val += 'libp11-native p11-kit-native'
    return val

DEPENDS:append = " ${@get_extra_depends(d)}"

export OPENSSL_ENGINES="${@get_openssl_engines(d)}"
export PKCS11_MODULE_PATH="${@get_pkcs11_module(d)}"
export P11_KIT_SERVER_ADDRESS
export P11_KIT_SERVER_PID
