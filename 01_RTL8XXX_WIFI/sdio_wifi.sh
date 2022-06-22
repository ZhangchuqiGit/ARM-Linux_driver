#/bin/bash

if [ -z "${Sudo}" ]; then
    Sudo="sudo"
fi
if [ "$(whoami)" = "root" ]; then
    Sudo=""
fi
cd_exit_echo() {
    cd "$*" || (
        echo -e "\033[5;7;32m cd $*\e[0m"
        exit 1
    )
}
debug_echo() {
    echo -e "\033[32m $* \e[0m"
}

debug_echo "打开 wlan0 网卡"
debug_echo "${Sudo} ifconfig wlan0 up "
${Sudo} ifconfig wlan0 up 

debug_echo "查找当前环境下的 WIFI 热点信息"
debug_echo "${Sudo} iwlist wlan0 scan &> /tmp/zcq_iwlist_wlan0_scan.txt"
${Sudo} iwlist wlan0 scan &> /tmp/zcq_iwlist_wlan0_scan.txt

# 要想连接到指定的 WIFI
# 热点上就需要用到 wpa_supplicant 工具

# ssid 是要连接的 WIFI 热点名字，这里我要连接的是“ZZK”这个 WIFI 热点。
# psk 就是要连接的 WIFI 热点密码，根据自己的实际情况填写即可。
wpa_supplicant='ctrl_interface=/var/run/wpa_supplicant
ap_scan=1
network={
 ssid="318"
 psk="520lab318"
}'
debug_echo "${wpa_supplicant}"
# 注意， wpa_supplicant.conf 文件对于格式要求比较严格，
# “=” 前后一定不能有空格，也不要用 TAB 键来缩进，
# 缩进应该采用空格，否则的话会出现文件解析错误！

wpa_supplicant_conf=/etc/wpa_supplicant.conf

${Sudo} echo "${wpa_supplicant}" &> ${wpa_supplicant_conf}

debug_echo cat ${wpa_supplicant_conf}
cat ${wpa_supplicant_conf}

debug_echo ${Sudo} mkdir -p -m 777 /var/run/wpa_supplicant
${Sudo} mkdir -p -m 777 /var/run/wpa_supplicant

debug_echo ${Sudo} wpa_supplicant -D nl80211 -c ${wpa_supplicant_conf} -i wlan0 # RTL8189
${Sudo} wpa_supplicant -D nl80211 -c ${wpa_supplicant_conf} -i wlan0  # RTL8189
# ${Sudo} wpa_supplicant -D wext -c ${wpa_supplicant_conf} -i wlan0 

debug_echo ${Sudo} udhcpc -i wlan0 # 从路由器获取 IP 地址
${Sudo} udhcpc -i wlan0 # 从路由器获取 IP 地址

debug_echo ifconfig wlan0
ifconfig wlan0

debug_echo "
如果 WIFI 工作正常的话就可以 ping 通百度网站
-I 指定 wlan0 的 IP 地址。
ping -I 192.168.6.200 www.baidu.com "
