inherit kernel-fitimage

#
# Emit the fitImage ITS configuration section
#
# $1 ... .its filename
# $2 ... Linux kernel ID
# $3 ... DTB image name(s)
# $4 ... ramdisk ID
# $5 ... u-boot script ID
# $6 ... config ID
# $7 ... default flag
fitimage_emit_section_config() {

    conf_csum="${FIT_HASH_ALG}"
    conf_sign_algo="${FIT_SIGN_ALG}"
    conf_padding_algo="${FIT_PAD_ALG}"
    if [ "${UBOOT_SIGN_ENABLE}" = "1" ] ; then
        conf_sign_keyname="${UBOOT_SIGN_KEYNAME}"
    fi

    its_file="$1"
    kernel_id="$2"
    dtb_images="$3"
    ramdisk_id="$4"
    bootscr_id="$5"
    config_id="$6"
    default_flag="$7"

    # Test if we have any DTBs at all
    sep=""
    conf_desc=""
    conf_node="${FIT_CONF_PREFIX}"
    kernel_line=""
    pri_fdt_line=""
    sec_fdt_line=""
    ramdisk_line=""
    bootscr_line=""
    setup_line=""
    default_line=""
    default_dtb_image="${FIT_CONF_DEFAULT_DTB}"

    conf_node=$conf_node$kernel_id

    if [ -n "$kernel_id" ]; then
        conf_desc="Linux kernel"
        sep=", "
        kernel_line="kernel = \"kernel-$kernel_id\";"
    fi

    if [ -n "$dtb_images" ]; then
        conf_desc="$conf_desc${sep}FDT blob"
        sep=", "

        set -- $dtb_images
        primary_dtb_image="$1"
        secondary_dtb_image="$2"

        pri_fdt_line="fdt = \"fdt-$primary_dtb_image\";"
        sec_fdt_line="fdt-secondary = \"fdt-$secondary_dtb_image\";"
    fi

    if [ -n "$ramdisk_id" ]; then
        conf_desc="$conf_desc${sep}ramdisk"
        sep=", "
        ramdisk_line="ramdisk = \"ramdisk-$ramdisk_id\";"
    fi

    if [ -n "$bootscr_id" ]; then
        conf_desc="$conf_desc${sep}u-boot script"
        sep=", "
        bootscr_line="bootscr = \"bootscr-$bootscr_id\";"
    fi

    if [ -n "$config_id" ]; then
        conf_desc="$conf_desc${sep}setup"
        setup_line="setup = \"setup-$config_id\";"
    fi

    if [ "$default_flag" = "1" ]; then
        # default node is selected based on dtb ID if it is present,
        # otherwise its selected based on kernel ID
        if [ -n "$dtb_image" ] && [ -n "$default_dtb_image" ]; then
            # Select default node as user specified dtb when
            # multiple dtb exists.
            if [ -s "${EXTERNAL_KERNEL_DEVICETREE}/$default_dtb_image" ]; then
                default_line="default = \"${FIT_CONF_PREFIX}$default_dtb_image\";"
            else
                bbwarn "Couldn't find a valid user specified dtb in ${EXTERNAL_KERNEL_DEVICETREE}/$default_dtb_image"
            fi
        else
            default_line="default = \"${FIT_CONF_PREFIX}$kernel_id\";"
        fi
    fi

    cat << EOF >> $its_file
        $default_line
        $conf_node {
            description = "$default_flag $conf_desc";
            $kernel_line
            $pri_fdt_line
            $sec_fdt_line
            $ramdisk_line
            $bootscr_line
            $setup_line
            hash-1 {
                algo = "$conf_csum";
            };
EOF

    if [ -n "$conf_sign_keyname" ] ; then

        sign_line="sign-images = "
        sep=""

        if [ -n "$kernel_id" ]; then
            sign_line="$sign_line${sep}\"kernel\""
            sep=", "
        fi

        if [ -n "$dtb_images" ]; then
            sign_line="$sign_line${sep}\"fdt\""
            sep=", "
            sign_line="$sign_line${sep}\"fdt-secondary\""
        fi

        if [ -n "$ramdisk_id" ]; then
            sign_line="$sign_line${sep}\"ramdisk\""
            sep=", "
        fi

        if [ -n "$bootscr_id" ]; then
            sign_line="$sign_line${sep}\"bootscr\""
            sep=", "
        fi

        if [ -n "$config_id" ]; then
            sign_line="$sign_line${sep}\"setup\""
        fi

        sign_line="$sign_line;"

        cat << EOF >> $its_file
            signature-1 {
                algo = "$conf_csum,$conf_sign_algo";
                key-name-hint = "$conf_sign_keyname";
                padding = "$conf_padding_algo";
                $sign_line
            };
EOF
    fi

    cat << EOF >> $its_file
        };
EOF
}

#
# Assemble fitImage
#
# $1 ... .its filename
# $2 ... fitImage name
# $3 ... include ramdisk
fitimage_assemble() {
    # [ADI] most of this function is copy-pasted from:
    # layers/poky/meta/classes/kernel-fitimage.bbclass
    # the configuration section at the bottom is modified.
    # unfortunately, the base class was not written to allow
    # overriding just part of this function.


    kernelcount=1
    dtbcount=""
    DTBS=""
    ramdiskcount=$3
    setupcount=""
    bootscr_id=""
    rm -f $1 arch/${ARCH}/boot/$2

    if [ -n "${UBOOT_SIGN_IMG_KEYNAME}" -a "${UBOOT_SIGN_KEYNAME}" = "${UBOOT_SIGN_IMG_KEYNAME}" ]; then
        bbfatal "Keys used to sign images and configuration nodes must be different."
    fi

    fitimage_emit_fit_header $1

    #
    # Step 1: Prepare a kernel image section.
    #
    fitimage_emit_section_maint $1 imagestart

    uboot_prep_kimage
    fitimage_emit_section_kernel $1 $kernelcount linux.bin "$linux_comp"

    #
    # Step 2: Prepare a DTB image section
    #

    if [ -n "${KERNEL_DEVICETREE}" ]; then
        dtbcount=1
        for DTB in ${KERNEL_DEVICETREE}; do
            if echo $DTB | grep -q '/dts/'; then
                bbwarn "$DTB contains the full path to the the dts file, but only the dtb name should be used."
                DTB=`basename $DTB | sed 's,\.dts$,.dtb,g'`
            fi

            # Skip ${DTB} if it's also provided in ${EXTERNAL_KERNEL_DEVICETREE}
            if [ -n "${EXTERNAL_KERNEL_DEVICETREE}" ] && [ -s ${EXTERNAL_KERNEL_DEVICETREE}/${DTB} ]; then
                continue
            fi

            DTB_PATH="arch/${ARCH}/boot/dts/$DTB"
            if [ ! -e "$DTB_PATH" ]; then
                DTB_PATH="arch/${ARCH}/boot/$DTB"
            fi

            DTB=$(echo "$DTB" | tr '/' '_')

            # Skip DTB if we've picked it up previously
            echo "$DTBS" | tr ' ' '\n' | grep -xq "$DTB" && continue

            DTBS="$DTBS $DTB"
            fitimage_emit_section_dtb $1 $DTB $DTB_PATH
        done
    fi

    if [ -n "${EXTERNAL_KERNEL_DEVICETREE}" ]; then
        dtbcount=1
        for DTB in $(find "${EXTERNAL_KERNEL_DEVICETREE}" -name '*.dtb' -printf '%P\n' | sort) \
        $(find "${EXTERNAL_KERNEL_DEVICETREE}" -name '*.dtbo' -printf '%P\n' | sort); do
            DTB=$(echo "$DTB" | tr '/' '_')

            # Skip DTB/DTBO if we've picked it up previously
            echo "$DTBS" | tr ' ' '\n' | grep -xq "$DTB" && continue

            DTBS="$DTBS $DTB"
            fitimage_emit_section_dtb $1 $DTB "${EXTERNAL_KERNEL_DEVICETREE}/$DTB"
        done
    fi

    #
    # Step 3: Prepare a u-boot script section
    #

    if [ -n "${UBOOT_ENV}" ] && [ -d "${STAGING_DIR_HOST}/boot" ]; then
        if [ -e "${STAGING_DIR_HOST}/boot/${UBOOT_ENV_BINARY}" ]; then
            cp ${STAGING_DIR_HOST}/boot/${UBOOT_ENV_BINARY} ${B}
            bootscr_id="${UBOOT_ENV_BINARY}"
            fitimage_emit_section_boot_script $1 "$bootscr_id" ${UBOOT_ENV_BINARY}
        else
            bbwarn "${STAGING_DIR_HOST}/boot/${UBOOT_ENV_BINARY} not found."
        fi
    fi

    #
    # Step 4: Prepare a setup section. (For x86)
    #
    if [ -e arch/${ARCH}/boot/setup.bin ]; then
        setupcount=1
        fitimage_emit_section_setup $1 $setupcount arch/${ARCH}/boot/setup.bin
    fi

    #
    # Step 5: Prepare a ramdisk section.
    #
    if [ "x${ramdiskcount}" = "x1" ] && [ "${INITRAMFS_IMAGE_BUNDLE}" != "1" ]; then
        # Find and use the first initramfs image archive type we find
        found=
        for img in ${FIT_SUPPORTED_INITRAMFS_FSTYPES}; do
            initramfs_path="${DEPLOY_DIR_IMAGE}/${INITRAMFS_IMAGE_NAME}.$img"
            if [ -e "$initramfs_path" ]; then
                bbnote "Found initramfs image: $initramfs_path"
                found=true
                fitimage_emit_section_ramdisk $1 "$ramdiskcount" "$initramfs_path"
                break
            else
                bbnote "Did not find initramfs image: $initramfs_path"
            fi
        done

        if [ -z "$found" ]; then
            bbfatal "Could not find a valid initramfs type for ${INITRAMFS_IMAGE_NAME}, the supported types are: ${FIT_SUPPORTED_INITRAMFS_FSTYPES}"
        fi
    fi

    fitimage_emit_section_maint $1 sectend

    # Force the first Kernel and DTB in the default config
    kernelcount=1
    if [ -n "$dtbcount" ]; then
        dtbcount=1
    fi

    #
    # Step 6: Prepare a configurations section
    #
    fitimage_emit_section_maint $1 confstart

    # kernel-fitimage.bbclass currently only supports a single kernel (no less or
    # more) to be added to the FIT image along with 0 or more device trees and
    # 0 or 1 ramdisk.
    # It is also possible to include an initramfs bundle (kernel and rootfs in one binary)
    # When the initramfs bundle is used ramdisk is disabled.
    # If a device tree is to be part of the FIT image, then select
    # the default configuration to be used is based on the dtbcount. If there is
    # no dtb present than select the default configuation to be based on
    # the kernelcount.

    # [ADI] Edited section begins

    if [ -n "$DTBS" ]; then
        fitimage_emit_section_config $1 $kernelcount "$DTBS" "$ramdiskcount" "$bootscr_id" "$setupcount" 1
    else
        defaultconfigcount=1
        fitimage_emit_section_config $1 $kernelcount "" "$ramdiskcount" "$bootscr_id"  "$setupcount" $defaultconfigcount
    fi

    # [ADI] Edited section ends

    fitimage_emit_section_maint $1 sectend

    fitimage_emit_section_maint $1 fitend

    #
    # Step 7: Assemble the image
    #
    ${UBOOT_MKIMAGE} \
        ${@'-D "${UBOOT_MKIMAGE_DTCOPTS}"' if len('${UBOOT_MKIMAGE_DTCOPTS}') else ''} \
        -f $1 \
        arch/${ARCH}/boot/$2

    #
    # Step 8: Sign the image and add public key to U-Boot dtb
    #
    if [ "x${UBOOT_SIGN_ENABLE}" = "x1" ] ; then
        # [ADI] Edited section begins
        ${UBOOT_MKIMAGE_SIGN} \
            ${@'-D "${UBOOT_MKIMAGE_DTCOPTS}"' if len('${UBOOT_MKIMAGE_DTCOPTS}') else ''} \
            -F -k "${UBOOT_SIGN_KEYDIR}" \
            -K "${B}/${UBOOT_DTB_BINARY}" \
            -r arch/${ARCH}/boot/$2 \
            ${UBOOT_MKIMAGE_SIGN_ARGS}
        # [ADI] Edited section ends
    fi
}
