#! /bin/bash

#### 获取设备节点： of_find_node_by_path("/gpioled");
#### 若驱动程序采用 设备树方式，设备树 接口： /sys/devices/platform/gpioled

################################################################

# 修改

# 应用
# DriversAPP=mini_linux_App    					# mini_linux_App.c
DriversAPP=

# 编译模块
# CODE_Dirver="hello.o mini_linux_module.o"		# hello.c mini_linux_module.c
CODE_Dirver=miscbeep.o

################################################################

# 驱动文件输出路劲
# SendPath=/mnt/hgfs/ARM_Linux_iTOP-4412/ZCQ_OUT_modules
# SendPath=../ZCQ_OUT_modules
SendPath=/home/zcq/ARM_nfs

# kernel 源码 目录
KERNELDIR=/home/zcq/ARM_Linux_iTOP-4412/zcq_itop4412_POP1G_kernel-4.14.2
# KERNELDIR=/home/zcq/ARM_Linux_iTOP-4412/zcq_itop4412_POP1G_kernel-5.3.18/kernel-5.3.18

# 工具链
# ToolchainPath=/home/zcq/Arm_tool_x86_64_linux/gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12
ToolchainPath=/home/zcq/Arm_tool_x86_64_linux/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf

# GCC=arm-none-linux-gnueabi-gcc
GCC=arm-none-linux-gnueabihf-gcc

export PATH=$PATH:${ToolchainPath}/bin

################################################################

################################################################

RootPath=$(pwd)
# DebugOutLog="zcq_debug.log" # 调试日志输出
# Clash_proxychains="proxychains4" # 使用代理
# Time_launch="$(date +%Y.%m.%d_%H.%M.%S)"
# Time_launch="$(date +%m.%d_%H.%M)"

if [ -z "${Sudo}" ]; then
	Sudo="sudo"
fi
if [ "$(whoami)" = "root" ]; then
	Sudo=""
fi

# 调试日志输出
if [ -n "${DebugOutLog}" ]; then
	DebugOutLog=${RootPath}/${DebugOutLog}
	if [ ! -f "${DebugOutLog}" ]; then
		${Sudo} rm -rf ${DebugOutLog}
	fi
	if [ ! -e "${DebugOutLog}" ]; then
		echo &>${DebugOutLog}
	fi
fi

if [ -z "${Clash_proxychains}" ]; then
	# Clash_proxychains="proxychains4"
	Clash_proxychains=" "
fi

func_log() {
	if [ -n "${DebugOutLog}" ] && [ -f "${DebugOutLog}"  ]; then
		local Time
		Time="$(date +%Y.%m.%d_%H.%M.%S)"
		echo "'${Time}' $*" &>>${DebugOutLog}
	fi
}

Echo_Num_x=30
Echo_Num_y=47
func_echo_loop() {
	if [ ${Echo_Num_x} -gt 37 ]; then
		Echo_Num_x=30
	fi
	if [ ${Echo_Num_y} -lt 40 ]; then
		Echo_Num_y=47
	fi
	echo -e "\033[1;7;${Echo_Num_x};${Echo_Num_y}m $* \e[0m"
	Echo_Num_x=$((${Echo_Num_x} + 1))
	Echo_Num_y=$((${Echo_Num_y} - 1))
}

func_echo() {
	local slt=$1
	case ${slt} in
		-v) # -variable 变量
			shift
			#echo -e " \033[3;7;37;44mVariable\e[0m (\033[37;44m $* \e[0m)"
			func_echo_loop "(Variable) $*"
			func_log "(Variable) $*"
			;;
		-p) # path or file
			shift
			echo -e " \033[3;7;36;40m[Path]\e[0m [\033[36;40m $* \e[0m]"
			func_log "[Path] $*"
			;;
		-e) # error 错误
			shift
			echo -e " \033[1;5;7;30;41m !!! 错误 \e[0m\033[30;41m $* \e[0m"
			func_log "!!! 错误  $*"
			;;
		-c) # shell 命令
			shift
			echo -e " \033[1;3;5;32m\$ \e[0m\033[1;32;40m $* \e[0m"
			func_log "$*"
			;;
		*) # 描述信息
			echo -e "\033[35m $* \e[0m"
			#func_echo_loop "$*"
			;;
	esac
	#	unset slt
}

# func_execute 执行语句 成功与否打印
func_execute() {
	func_echo -c "$*"
	$@
}
func_execute_sudo() {
	func_execute "${Sudo} $*"
}
func_execute_err() {
	$?=0
	func_execute "$@"
	local ret=$?
	if [ $ret -ne 0 ]; then
		func_echo -e "ret=$ret 执行 $*"
		exit $ret
	fi
}
func_execute_err_sudo() {
	func_execute_err "${Sudo} $*"
}
# func_execute pwd ; ifconfig
# func_execute mkdir -m 777 -p -v file # 创建文件夹

## 安装、更新  *.dep
func_apt() {
	#	declare -i mode
	local mode=" "
	while [ $# -gt 0 ]; do
		case "$1" in
			-a)
				func_execute_sudo apt autoremove -y
				func_execute_sudo apt autoclean -y
				;;
			-d)
				func_execute ${Sudo} ${Clash_proxychains} apt update -y
				func_execute ${Sudo} ${Clash_proxychains} apt update -y --fix-missing
				;;
			-g)
				func_execute ${Sudo} ${Clash_proxychains} apt upgrade -y
				;;
			-f)
				func_execute_sudo ${Clash_proxychains} apt install -y -f
				;;
			-i)
				mode="-i"
				;;
			-ii)
				mode="-ii"
				;;
			-r)
				mode="-r"
				;;
			*)
				if [ "${mode}" = "-i" ]; then
					func_execute_sudo ${Clash_proxychains} apt install -y $1 -f
				elif [ "${mode}" = "-ii" ]; then
					func_execute_sudo ${Clash_proxychains} aptitude install -y $1 -f
				elif [ "${mode}" = "-r" ]; then
					func_execute_sudo apt remove -y --purge $1
				fi
				;;
		esac
		shift
	done
}

func_apt_repository() {
	#	Examples:
	#        apt-add-repository 'deb http://myserver/path/to/repo stable myrepo'
	#        apt-add-repository 'http://myserver/path/to/repo myrepo'
	#        apt-add-repository 'https://packages.medibuntu.org free non-free'
	#        apt-add-repository http://extras.ubuntu.com/ubuntu
	#        apt-add-repository ppa:user/repository
	#        apt-add-repository ppa:user/distro/repository
	#        apt-add-repository multiverse
	while [ $# -gt 0 ]; do
		#		func_echo -c "${Sudo} ${Clash_proxychains} add-apt-repository $1"
		func_execute_sudo ${Clash_proxychains} add-apt-repository -y "$1"
		func_apt -d -f
		shift
	done
}

func_cd() {
	func_echo -c "cd $1"
	cd $1 || exit
}

func_gedit() {
	while [ $# -gt 0 ]; do
		func_echo -c "${Sudo} gedit $1 &>/dev/null"
		${Sudo} gedit $1 &>/dev/null
		shift
	done
}

func_cpf() {
	local source="$1"
	local target="$2"
	# $(dirname $Rootpath)  # 取出目录: /home/.../rootfs.../
	# $(basename $Rootpath) # 取出文件: zcq_Create_rootfs
	func_execute_sudo cp -raf "${source}" "${target}"
}

func_cp() {
	local source="$1"
	local target="$2"
	func_execute_sudo cp -ra "${source}" "${target}"
}

func_cpl() {
	local source="$1"
	local target="$2"
	func_execute_sudo cp -rad "${source}" "${target}"
}

func_dpkg() {
	# for file in $(ls *.deb)
	# sudo dpkg -i *.deb
	func_apt -d -f
	func_execute_sudo dpkg --add-architecture i386 # 准备添加32位支持
	func_execute_sudo dpkg --configure -a
	while [ $# -gt 0 ]; do
		func_execute_sudo dpkg -i $1
		func_apt -f
		shift
	done
}

func_chmod() {
	func_execute_sudo chmod -R 777 "$@"
}
# func_chmod ${RootPath}

func_path_isexit () {
	local targetpath="$2"
	if [ -d "${targetpath}" ]; then
		func_echo "$1 ${targetpath}"
	else
		func_echo -e "$1 ${targetpath}"
		exit
	fi
}

func_echo "使用代理    ${Clash_proxychains}"
func_echo "调试日志输出 ${DebugOutLog}"
func_echo "当前路径    ${RootPath}"

################################################################

################################################################

func_chmod ${RootPath}
func_cd ${RootPath}

func_path_isexit "工具链路径" ${ToolchainPath}
func_chmod ${ToolchainPath}

func_echo "GCC           " ${GCC}
func_echo "应用           " ${DriversAPP}
func_echo "编译模块        " ${CODE_Dirver}
func_echo "kernel 源码目录 " ${KERNELDIR}
func_echo "驱动文件输出路劲 " ${SendPath}

# if [ -d "${SendPath}" ]; then
#     func_execute_sudo rm -rf ${SendPath}
# fi
func_execute_sudo mkdir -m 777 -p ${SendPath} # 创建文件夹

if [ -n "${DriversAPP}" ] && [ -f "${DriversAPP}.c" ]; then
	${GCC}  ${DriversAPP}.c -o ${DriversAPP} -static
	# ${GCC}  ${DriversAPP}.c -o ${DriversAPP} 
	func_execute mv -f ${DriversAPP} ${SendPath}/
fi

# CpuCoreNum=$(grep processor /proc/cpuinfo  | awk '{num=$NF+1};END{print num}') # 获取内核数目
# CpuCoreNum=$(cat /proc/cpuinfo | grep "processor" | wc -l) # 获取内核数目
CpuCoreNum=$(cat /proc/cpuinfo | grep -c "processor") # 获取内核数目
func_echo "获取内核数目 ${CpuCoreNum}"

func_echo -c "
make build -j${CpuCoreNum} 		
	KERNELDIR=${KERNELDIR} 
	CODE_Dirver=\"${CODE_Dirver}\" 				
	CURRENT_PATH=${RootPath}     "

make build -j${CpuCoreNum} 		\
	KERNELDIR=${KERNELDIR}					\
	CODE_Dirver="${CODE_Dirver}" 				\
	CURRENT_PATH=${RootPath}

func_cpf "*.ko" ${SendPath}/

func_chmod ${SendPath}

func_echo -c "
make clean
	KERNELDIR=${KERNELDIR} 
	CODE_Dirver=\"${CODE_Dirver}\" 				
	CURRENT_PATH=${RootPath}     "

make clean					\
	KERNELDIR=${KERNELDIR}					\
	CODE_Dirver="${CODE_Dirver}" 				\
	CURRENT_PATH=${RootPath}


###############################################################################

#  ${file#*/}：删掉第一个 / 及其左边的字符串：dir1/dir2/dir3/my.file.txt
#  ${file##*/}：删掉最后一个 /  及其左边的字符串：my.file.txt
#  ${file#*.}：删掉第一个 .  及其左边的字符串：file.txt
#  ${file##*.}：删掉最后一个 .  及其左边的字符串：txt

#  ${file%/*}：删掉最后一个  /  及其右边的字符串：/dir1/dir2/dir3 变 /dir1/dir2
#  ${file%%/*}：删掉第一个 /  及其右边的字符串：(空值)
#  ${file%.*}：删掉最后一个  .  及其右边的字符串：/dir1/dir2/dir3/my.file
#  ${file%%.*}：删掉第一个  .   及其右边的字符串：/dir1/dir2/dir3/my
