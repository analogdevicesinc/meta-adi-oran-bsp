inherit image_types_wic

IMAGE_CMD:wic () {
	out="${IMGDEPLOYDIR}/${IMAGE_NAME}"
	build_wic="${WORKDIR}/build-wic"
	tmp_wic="${WORKDIR}/tmp-wic"
	wks="${WKS_FULL_PATH}"
	if [ -e "$tmp_wic" ]; then
		# Ensure we don't have any junk leftover from a previously interrupted
		# do_image_wic execution
		rm -rf "$tmp_wic"
	fi
	if [ -z "$wks" ]; then
		bbfatal "No kickstart files from WKS_FILES were found: ${WKS_FILES}. Please set WKS_FILE or WKS_FILES appropriately."
	fi
	BUILDDIR="${TOPDIR}" PSEUDO_UNLOAD=1 wic create "$wks" --vars "${STAGING_DIR}/${MACHINE}/imgdata/" -e "${IMAGE_BASENAME}" -o "$build_wic/" -w "$tmp_wic" ${WIC_CREATE_EXTRA_ARGS}

	# look to see if the user specifies a custom imager
	IMAGER=direct
	eval set -- "${WIC_CREATE_EXTRA_ARGS} --"
	while [ 1 ]; do
		case "$1" in
			--imager|-i)
				shift
				IMAGER=$1
				;;
			--)
				shift
				break
				;;
		esac
		shift
	done
	mv "$build_wic/$(basename "${wks%.wks}")"*.direct "$out.wic"
}
IMAGE_CMD:wic[vardepsexclude] = "WKS_FULL_PATH WKS_FILES TOPDIR"
do_image_wic[cleandirs] = "${WORKDIR}/build-wic"