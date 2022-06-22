#/bin/bash

TOOLCHAIN=/home/zcq/Arm_tool_x86_64_linux/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
# TOOLCHAIN=/home/zcq/Arm_tool_x86_64_linux/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf
# TOOLCHAIN=/home/zcq/Arm_tool_x86_64_linux/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf

KERNELDIR=arm-linux-gnueabihf
# KERNELDIR=arm-linux-gnueabihf
# KERNELDIR=arm-none-linux-gnueabihf

export PATH=$TOOLCHAIN/bin:$PATH

Rootpath=$(pwd)
sudo chmod -R 777 ${Rootpath}

myprefix=${Rootpath}/zcq_prefix
rm -rf ${myprefix}

##########################################################################
# cpu_cores=$(grep processor /proc/cpuinfo  | awk '{num=$NF+1};END{print num}') # 获取内核数目
# cpu_cores=$(cat /proc/cpuinfo | grep "processor" | wc -l) # 获取内核数目
cpu_cores=$(cat /proc/cpuinfo | grep -c "processor") # 获取内核数目
echo "\033[32m 获取内核数目: $cpu_cores \e[0m"
##########################################################################
cd_exit_echo() {
    cd "$*" || (
        echo "\033[5;7;32m cd $*\e[0m"
        exit 1
    )
}
debug_echo() {
    echo "\033[5;32m ----------------------------------------------
$* \e[0m"
}
##########################################################################

if [ -f Makefile ]; then
    debug_echo "make distclean -j${cpu_cores} "
    make distclean -j${cpu_cores} 
fi

debug_echo "./configure '\'
    --prefix=${myprefix} '\'
    --host=${KERNELDIR} "
# host 用于指定交叉编译器。配置成功以后会生成 Makefile
# -prefix=编译输出路径
./configure  \
    --prefix=${myprefix} \
    --host=${KERNELDIR}

if true; then
    debug_echo "make -j${cpu_cores} || exit 1"
    make -j${cpu_cores} || exit 1
else
    debug_echo "make || exit 1"
    make || exit 1
fi

debug_echo "make install || exit 1"
make install || exit 1

# debug_echo "make distclean &>/dev/null"
# make distclean &>/dev/null

debug_echo "ok"

sudo chmod -R 777 ${Rootpath}

