EXTRA_IMAGEDEPENDS += "qemu-system-native qemu-helper-native:do_addto_recipe_sysroot"
IMAGE_CLASSES += "qemuboot"

# For runqemu
QB_SYSTEM_NAME = "qemu-system-aarch64"
QB_MACHINE = "-machine virt"
QB_CPU = "-cpu cortex-a57"
QB_SMP = "-smp 4"
QB_DEFAULT_FSTYPE = "ext3"
QB_CPU_KVM = "-cpu host -machine gic-version=3"

# Virtio Networking support
QB_TAP_OPT = "-netdev tap,id=net0,ifname=@TAP@,script=no,downscript=no"
QB_NETWORK_DEVICE = "-device virtio-net-pci,netdev=net0,mac=@MAC@"

# Virtio serial console
QB_SERIAL_OPT = "-device virtio-serial-pci -chardev null,id=virtcon -device virtconsole,chardev=virtcon"
QB_TCPSERIAL_OPT = "-device virtio-serial-pci -chardev socket,id=virtcon,port=@PORT@,host=127.0.0.1 -device virtconsole,chardev=virtcon"

QB_KERNEL_CMDLINE_APPEND = "uio_pdrv_genirq.of_id=generic-uio earlyprintk"
