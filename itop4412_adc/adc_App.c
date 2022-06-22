/** ADC使用的是SOC自带的功能，一般SOC厂家已经把相应的驱动代码写好，
	我们只需要在设备树中使能该功能则可。在进行ADC读操作时，
	只需要了解对IIO子系统的使用操作，即可完成ADC的读取

	编译并烧写内核，启动后即可在终端下运行以下命令来读取 ADC0 的值
	数据采集的过程中，旋转电位器的旋钮，改变电位器的电阻分压，就会改变转换后的结果。
	cat /sys/devices/platform/126c0000.adc/iio:device0/in_voltage0_raw
	cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw
	/dev/iio:device0

	itop4412 一共有 4 路 ADC 接口
	* 网络标号是 XadcAIN0~XadcAIN3 
	* 开发板自带的 ADC 电路， ADC 接的是滑动变阻器
	Power Supply Voltage: 1.8V (Typ.), 1.0V (Typ., Digital I/O Interface)
	Analog Input Range: 0 ~ 1.8V 
	Exynos 4412 has two ADC blocks, ADC_CFG[16] setting :
		General ADC : 0x126C_0000
		MTCADC_ISP :  0x1215_0000  
	sec_exynos4412_users manual_ver.1.00.00.pdf  
	57 ADC 57.7	p2770

	/proc/device-tree/adc@126C0000 
	
	adc: adc@126C0000 {
		compatible = "samsung,exynos-adc-v1";
		reg = <0x126C0000 0x100>;
		interrupt-parent = <&combiner>;
		interrupts = <10 3>;
		clocks = <&clock CLK_TSADC>;
		clock-names = "adc";
		#io-channel-cells = <1>;
		io-channel-ranges;
		samsung,syscon-phandle = <&pmu_system_controller>;
		status = "disabled";
	};
	*/

#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

/* adc iio 框架对应的文件路径 */
char *filename = "/sys/bus/iio/devices/iio:device0/in_voltage0_raw";

int main(int argc, char *argv[])
{
	int ret = 0;
	FILE *data_stream = NULL;
	char str[16];

	data_stream = fopen(filename, "r"); /* 只读打开 */
	if (data_stream == NULL) {
		printf("can't open file %s\r\n", filename);
		return -1;
	}

	if (argc != 1) {
		printf("Error Usage!\r\n");
		return -1;
	}

	while (1) {
		memset(str, 0, sizeof(str));
		ret = fscanf(data_stream, "%s", str);
		if (!ret) {
			printf("file read error!\r\n");
		}
		else if (ret == EOF) {
			/* 读到文件末尾的话将文件指针重新调整到文件头 */
			fseek(data_stream, 0, SEEK_SET);
		}

		ret = atoi(str);
		printf("ADC原始值：%d，电压值：%.3fV\r\n", ret, ret * 1.80 / 4096);
		usleep(100000); /*100ms */
	}

	fclose(data_stream); /* 关闭文件 */
	return 0;
}
