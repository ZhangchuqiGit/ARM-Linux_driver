#/bin/bash

# 交叉编译器
# TOOLCHAIN_GCC=/home/zcq/Arm_tool_x86_64_linux/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
TOOLCHAIN_GCC=/home/zcq/Arm_tool_x86_64_linux/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
# TOOLCHAIN_GCC=/home/zcq/Arm_tool_x86_64_linux/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc

Rootpath=$(pwd)
sudo chmod -R 777 ${Rootpath}

# openssl 库 和 头文件 路径
DIR_openssl=/home/zcq/ARM_kernel-5.3.18_driver_itop4412/01_RTL8XXX_WIFI/openssl-OpenSSL_1_1_1l/zcq_prefix

# libnl 库 和 头文件 路径
DIR_libnl=/home/zcq/ARM_kernel-5.3.18_driver_itop4412/01_RTL8XXX_WIFI/libnl-3.2.23/zcq_prefix

# 指定 libnl 库 pkgconfig 包位置
export PKG_CONFIG_PATH=${DIR_libnl}/lib/pkgconfig:${PKG_CONFIG_PATH}

VAL_config="
# 配置文件 :
# 指定 交叉编译器、openssl、libnl 库 和 头文件 路径
# 需要添加 如 : 

# 交叉编译器:
CC = ${TOOLCHAIN_GCC}
# LIBS += -lpthread

# openssl 库 和 头文件 路径:
CFLAGS += -I${DIR_openssl}/include
LIBS += -L${DIR_openssl}/lib -lssl -lcrypto

# libnl 库 和 头文件 路径
CFLAGS += -I${DIR_libnl}/include/libnl3
LIBS += -L${DIR_libnl}/lib
"

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
    echo "\033[32m ----------------------------------------------
$* \e[0m"
}
##########################################################################

# cd_exit_echo ${Rootpath}
cd_exit_echo wpa_supplicant

debug_echo ${VAL_config}

if true; then
    echo "${VAL_config}" > .config
    cat defconfig | cat >> .config
else
    cp -f defconfig .config
    gedit .config &>/dev/null
fi

# if [ -f Makefile ]; then
#     debug_echo "make clean -j${cpu_cores} "
#     make clean -j${cpu_cores}
# fi

if false; then
    debug_echo "make -j${cpu_cores} || exit 1"
    make -j${cpu_cores} || exit 1
else
    debug_echo "make || exit 1"
    make || exit 1
fi

debug_echo "
wpa_cli 和 wpa_supplicant 这两个文件拷贝到开发板根文件系统的 /usr/bin/
拷贝完成以后重启开发板！
输入 wpa_supplicant -v 命令查看版本号，工作正常就会打印出版本号 "

# debug_echo "make distclean &>/dev/null"
# make distclean &>/dev/null

debug_echo "ok"

sudo chmod -R 777 ${Rootpath}
