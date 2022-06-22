#!/usr/bin/bash

rootpath=$(pwd)

function __debug_echo() {
	local slt=$1
	case $slt in
		-v) # -variable 变量
			shift
			echo -e "\033[37;44m $* \e[0m"
			;;
		-p) # path or file
			shift
			echo -e "\033[36;40m $* \e[0m"
			;;
		-e) # error 错误
			shift
			echo -e "\033[30;41m 错误！$* \e[0m"
			;;
		-c) # shell 命令
			shift
			echo -e "\033[32m $* \e[0m"
			;;
		*) # 描述信息
			echo -e "\033[33m $* \e[0m"
			;;
	esac
	unset slt
}

# execute 执行语句 成功与否打印
execute() {
	__debug_echo -c "\$ $*"
	$@
	if [ $? -ne 0 ]; then
		__debug_echo -e "执行 $*"
		exit $?
	fi
}
# execute pwd ; ifconfig
# execute mkdir -m 777 -p -v file # 创建文件夹

# 排除的文件夹
declare -a exclude_file
declare -i num_i
num_i=0
exclude_file_func() {
	while [ $# -gt 0 ]; do
		#		case $1 in
		#			--help | -h)
		#				shift
		#				;;
		#			*)
		#				shift
		#				;;
		#		esac
		exclude_file[${num_i}]=$1
		shift
		let 'num_i++'
	done
}
# echo ${#exclude_file[@]} # 数组个数

exclude_file_compare() {
	compare_flag=false
	for ((i = 0; i < ${#exclude_file[@]}; i++)); do
		if [ ${exclude_file[$i]} == $1 ]; then
			compare_flag=true
		fi
	done
}

# search_file() {
# 	local arm_file=$1/
# 	local path_tmp=$(ls -1 $arm_file) # 多个文件夹
# 	for tmp in ${path_tmp}; do
# 		if [ -d $arm_file${tmp} ]; then
# 			exclude_file_compare ${tmp}
# 			if [ ${compare_flag} == false ]; then
# 				echo $arm_file${tmp} >>${zcq_file}
# 				search_file $arm_file${tmp}
# 			fi
# 		fi
# 	done
# }

search_file_code() {
	local arm_file=$1/
	local path_tmp=$(ls -1 $arm_file) # 多个文件夹
	for tmp in ${path_tmp}; do
		if [ -d $arm_file${tmp} ]; then
			exclude_file_compare ${tmp}
			if [ ${compare_flag} == false ]; then
				echo $arm_file${tmp} >>${zcq_file_Clion}
				echo "\"$arm_file${tmp}/\"," >>${zcq_file_VScode}
				search_file_code $arm_file${tmp}
			fi
		fi
	done
}

################################    修改处    ###################################

zcq_file_Clion=${rootpath}/zcq_file_Clion.txt
zcq_file_VScode=${rootpath}/zcq_file_VScode.txt
rm -rf ${zcq_file_Clion} ${zcq_file_VScode}

# 排除的文件夹
exclude_file_func cmake-build-debug .vscode .idea

# arch/arm
# exclude_file_func mach-actions mach-alpine mach-artpec mach-asm9260 
# exclude_file_func mach-aspeed mach-at91 mach-axxia mach-bcm mach-berlin 
# exclude_file_func mach-clps711x mach-cns3xxx mach-davinci mach-digicolor
# exclude_file_func mach-dove mach-ebsa110 mach-efm32 mach-ep93xx 
# exclude_file_func mach-footbridge mach-gemini mach-highbank mach-hisi mach-imx 
# exclude_file_func mach-integrator mach-iop13xx mach-iop32x mach-iop33x mach-ixp4xx
# exclude_file_func mach-keystone mach-ks8695 mach-lpc18xx mach-lpc32xx mach-mediatek 
# exclude_file_func mach-meson mach-mmp mach-moxart mach-mv78xx0 mach-mvebu mach-mxs mach-netx 
# exclude_file_func mach-nomadik mach-nspire mach-omap1 mach-omap2 mach-orion5x mach-oxnas  
# exclude_file_func mach-picoxcell mach-prima2 mach-pxa mach-qcom mach-realview mach-rockchip 
# exclude_file_func mach-rpc mach-s3c24xx mach-s3c64xx mach-s5pv210 mach-sa1100
# exclude_file_func mach-shmobile mach-socfpga mach-spear mach-sti mach-stm32 mach-sunxi
# exclude_file_func mach-tango mach-tegra mach-u300 mach-uniphier mach-ux500 mach-versatile
# exclude_file_func mach-vexpress mach-vt8500 mach-w90x900 mach-zx mach-zynq
# exclude_file_func plat-iop plat-omap plat-orion plat-pxa plat-versatile



search_file_code ${rootpath}
# search_file_code ${rootpath}/arch/arm 
# search_file_code ${rootpath}/drivers 
# search_file_code ${rootpath}/fs
# search_file_code ${rootpath}/include
# search_file_code ${rootpath}/kernel
# search_file_code ${rootpath}/lib
# search_file_code ${rootpath}/net scripts
# search_file_code ${rootpath}/scripts
# search_file_code ${rootpath}/sound
# search_file_code ${rootpath}/tools/arch/arm



echo ${rootpath//\//\\\/}
# sed -i -E -e "s/${rootpath//\//\\\/}/./g" ${zcq_file}
sed -i -E -e "s/${rootpath//\//\\\/}/\${workspaceFolder}/g" ${zcq_file}

# search_file_code $(pwd)/arch/arm
# search_file_code $(pwd)/board/samsung
# search_file_code $(pwd)/cmd
# search_file_code $(pwd)/common
# search_file_code $(pwd)/drivers
# search_file_code $(pwd)/dts
# search_file_code $(pwd)/fs
# search_file_code $(pwd)/include
# search_file_code $(pwd)/lib
# search_file_code $(pwd)/net
# search_file_code $(pwd)/post
# search_file_code $(pwd)/scripts
# search_file_code $(pwd)/test
# search_file_code $(pwd)/tools
