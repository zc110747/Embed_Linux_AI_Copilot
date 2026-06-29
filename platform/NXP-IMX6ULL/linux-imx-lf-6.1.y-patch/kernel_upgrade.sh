
# path
SRC_DIR=$(pwd)
DST_DIR=${AI_SDK_SOC_PATH}/kernel

#dts
cp -rv ${SRC_DIR}/arch/arm/boot/dts/* ${DST_DIR}/arch/arm/boot/dts/

#config
cp -rv ${SRC_DIR}/arch/arm/configs/* ${DST_DIR}/arch/arm/configs/

#firmware
if [ ! -d ${DST_DIR}/firmware ]; then
    mkdir ${DST_DIR}/firmware
fi
cp -rv ${SRC_DIR}/firmware/* ${DST_DIR}/firmware/

#drivers
cp -rv ${SRC_DIR}/drivers/* ${DST_DIR}/drivers/

#include
cp -rv ${SRC_DIR}/include/* ${DST_DIR}/include/

# scripts, support for overlay
cp -rv ${SRC_DIR}/scripts/* ${DST_DIR}/scripts/

#.vscode
if [ ! -d ${DST_DIR}/.vscode ]; then
    mkdir ${DST_DIR}/.vscode
fi
cp -rv ${SRC_DIR}/.vscode/settings.json ${DST_DIR}/.vscode/