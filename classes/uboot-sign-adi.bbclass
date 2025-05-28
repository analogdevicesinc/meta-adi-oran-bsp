inherit uboot-sign

do_uboot_assemble_fitimage() {
	if [ -n "${UBOOT_CONFIG}" ]; then
		unset i
		for config in ${UBOOT_MACHINE}; do
			unset j k
			i=$(expr $i + 1);
			for type in ${UBOOT_CONFIG}; do
				j=$(expr $j + 1);
				if [ $j -eq $i ]; then
					break;
				fi
			done

			for binary in ${UBOOT_BINARIES}; do
				k=$(expr $k + 1);
				if [ $k -eq $i ]; then
					break;
				fi
			done

			cd ${B}/${config}
			uboot_assemble_fitimage_helper ${type} ${binary}
		done
	else
		cd ${B}
		uboot_assemble_fitimage_helper "" ${UBOOT_BINARY}
	fi
}

deltask uboot_assemble_fitimage
addtask uboot_assemble_fitimage before do_deploy after do_install do_compile