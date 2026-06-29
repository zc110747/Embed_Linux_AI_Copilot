function help()
{
    echo
    echo "Usage:"
    echo "  AiQemu [sub-command]"
    echo "  ./build-qemu.sh [sub-command]"
    echo 
    echo "sub-command:  -h | -s | -u | -a | -r"
    echo
    echo "Example:"
    echo
    echo "AiQemu -h             --- help, show command help"
    echo "AiQemu -s             --- sample, only build qemu"
    echo "AiQemu -u             --- update, update and build qemu"
    echo "AiQemu -a             --- all, configuration、update、build qemu"
    echo "AiQemu -r             --- run, start run qemu"
}

function compile_sample()
{
    cd ${AI_SDK_QEMU_PATH}/

    make -j${COMPILE_CPU_CORE}
}

function compile_update()
{
    cd ${AI_PLATFORM_SOC_PATH}/qemu-11.0.0-patch

    # 更新qemu代码
    ./qemu_upgrade.sh

    # 进行编译
    compile_sample
}

function compile_all()
{
    cd ${AI_SDK_QEMU_PATH}/

    ./configure --prefix=$PWD/ --target-list="arm-softmmu arm-linux-user" --enable-debug --enable-sdl \
        --enable-slirp --enable-kvm --enable-tools --disable-curl --disable-coreaudio \
        --disable-pa --disable-sdl --disable-sndio
    
    compile_update
}

function qemu_run()
{
    kernel_file="${AI_SDK_SOC_PATH}/package/zImage"
    dtb_file="${AI_SDK_SOC_PATH}/package/imx6ull-qemu.dtb"
    rootfs_file="${AI_SDK_SOC_PATH}/package/${AI_IMG}"

    # nogui
    # qemu_tools=qemu-system-arm
    qemu_tools=${AI_SDK_QEMU_PATH}/build/qemu-system-arm
    sudo ${qemu_tools} -M mcimx6ul-evk -m 512M -kernel "${kernel_file}" \
        -dtb "${dtb_file}"  \
        -nographic \
        -serial mon:stdio \
        -drive  file="${rootfs_file}",format=raw,id=mysdcard -device sd-card,drive=mysdcard \
        -append "console=ttymxc0,115200 rootfstype=ext4 root=/dev/mmcblk1 rw rootwait init=/sbin/init loglevel=8" \
        -monitor telnet:127.0.0.1:4444,server,nowait \
        -nic user,hostfwd=tcp::2222-:22
}

function process_args()
{
    case $1 in
        *help|--h|-h)
            help
            exit 0
            ;;
        sample|--s|-s)
            compile_sample
            exit 0
            ;;
        all|--a|-a)
            compile_all
            exit 0
            ;;
        update|--u|-u)
            compile_update
            exit 0
            ;;
        run|--r|-r)
            qemu_run
            exit 0
            ;;
        esac
}

process_args $*