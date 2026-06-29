
PLATFORM_BOOT=$(pwd)

SRC_DIR=${PLATFORM_BOOT}
DST_DIR=${AI_SDK_QEMU_PATH}

cp -rv ${SRC_DIR}/hw/* ${DST_DIR}/hw/

cp -rv ${SRC_DIR}/include/* ${DST_DIR}/include/

cp -rv ${SRC_DIR}/monitor/* ${DST_DIR}/monitor/

cp -rv *.hx ${DST_DIR}/