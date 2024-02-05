QUARTUS_VER ?= "21.3"
QUARTUS_ROOT ?= "/opt"

SOF_FILE ?= "${QUARTUS_PROJ_NAME}.sof"

load_quartus() {
    export PATH=${QUARTUS_ROOT}/intelFPGA_pro/${QUARTUS_VER}/qprogrammer/quartus/bin:$PATH

    if ! command -v quartus_pfg; then
        bbfatal "Missing Quartus binaries. Please install the Quartus tool version ${QUARTUS_VER} at ${QUARTUS_ROOT}/intelFPGA_pro/${QUARTUS_VER}/qprogrammer/quartus/bin required for ${MACHINE}"
        exit 1
    fi
}
