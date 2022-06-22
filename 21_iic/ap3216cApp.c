#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: ap3216cApp.c 光传感器数据
作者	  	: 左忠凯
版本	   	: V1.0
描述	   	: ap3216c设备测试APP。
其他	   	: 无
使用方法	 ：./ap3216cApp /dev/ap3216c
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2019/9/20 左忠凯创建
***************************************************************/

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd;
	char *filename;
	unsigned short databuf[4];
	unsigned short ir, als, ps, ps_mode;
	int ret = 0;

	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if (fd < 0) {
		printf("can't open file %s\r\n", filename);
		return -1;
	}

	while (1) {
		ret = read(fd, databuf, sizeof(databuf));
		if (ret == 0) {            /* 数据读取成功 */
			ir = databuf[0];    /* ir传感器数据 */
			als = databuf[1];    /* als传感器数据 */
			ps = databuf[2];    /* ps传感器数据 */
			ps_mode = databuf[3];
//			printf("数字环境光传感器（ALS），红外LED（IR），距离传感器（PS）\r\n"
			printf(" ALS = %d, IR = %d, ps_mode:%d, PS = %d\r\n",
				   als, ir, ps_mode, ps);
		}
		usleep(200000); /*200ms */
	}
	close(fd);    /* 关闭文件 */
	return 0;
}
