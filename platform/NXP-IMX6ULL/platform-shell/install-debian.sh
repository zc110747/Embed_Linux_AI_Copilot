################################################################################
#
# This file is used to Install Debian System In address
# 
#
################################################################################
# define global environment
chip_arch=armhf
qemu_arch=arm
opt_debian_ver=bookworm
debian_dir=${AGENT_SDK_SOC_PATH}/rootfs/debian

# install support
sudo apt-get install debootstrap debian-archive-keyring qemu-user-static -y

# functional region
run_as_client() {
    $@ > /dev/null 2>&1
}

exit_if_last_error() {
    if [[ $? -ne 0 ]]; then
		read -p "shell faild, wheather exit?(y/n, default is y)" exit_val

		if [ -z ${exit_val} ] || [ ${exit_val} == 'y' ]; then
        	exit 1
		fi
    fi
}

mount_chroot()
{
    sudo mount -t proc chproc "${debian_dir}"/proc
    sudo mount -t sysfs chsys "${debian_dir}"/sys
    sudo mount -t devtmpfs chdev "${debian_dir}"/dev || sudo mount --bind /dev "${debian_dir}"/dev
    sudo mount -t devpts chpts "${debian_dir}"/dev/pts
}

umount_chroot()
{
    while grep -Eq "${debian_dir}.*(dev|proc|sys)" /proc/mounts
    do
        sudo umount -l --recursive "${debian_dir}"/dev >/dev/null 2>&1
        sudo umount -l "${debian_dir}"/proc >/dev/null 2>&1
        sudo umount -l "${debian_dir}"/sys >/dev/null 2>&1
        sleep 5
    done
}

# download debian from 
echo "Start Install Debian, platform:${greenText}${chip_arch}, ${qemu_arch}${defText}"
echo "Address:${greenText}${debian_dir}${defText}"

if [ ! -d "${debian_dir}"/bin/ ]; then
    sudo debootstrap --foreign --verbose --arch=${chip_arch} ${opt_debian_ver} "${debian_dir}" https://mirrors.tuna.tsinghua.edu.cn/debian/
else
    echo "Debian already Install."
    exit 0
fi
#exit_if_last_error

sudo chmod -Rv 777 "${debian_dir}"/usr/

cp /usr/bin/qemu-${qemu_arch}-static "${debian_dir}"/usr/bin/
chmod +x "${debian_dir}"/usr/bin/qemu-${qemu_arch}-static

#基于debootstrap完成后续安装
cd "${debian_dir}"/

mount_chroot
LC_ALL=C LANGUAGE=C LANG=C sudo chroot "${debian_dir}" /debootstrap/debootstrap --second-stage --verbose
LC_ALL=C LANGUAGE=C LANG=C sudo chroot "${debian_dir}" apt-get install vim libatomic1 -y
umount_chroot
