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
文件名		: icm20608App.c
作者	  	: 左忠凯
版本	   	: V1.0
描述	   	: icm20608设备测试APP。
其他	   	: 无
使用方法	 ：./icm20608App /dev/icm20608
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
	signed int databuf[7];
	unsigned char data[14];
	signed int gyro_x_adc, gyro_y_adc, gyro_z_adc;
	signed int accel_x_adc, accel_y_adc, accel_z_adc;
	signed int temp_adc;

	float gyro_x_act, gyro_y_act, gyro_z_act;
	float accel_x_act, accel_y_act, accel_z_act;
	float temp_act;

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
			gyro_x_adc = databuf[0];
			gyro_y_adc = databuf[1];
			gyro_z_adc = databuf[2];
			accel_x_adc = databuf[3];
			accel_y_adc = databuf[4];
			accel_z_adc = databuf[5];
			temp_adc = databuf[6];

			/* 计算实际值 */
			gyro_x_act = (float) (gyro_x_adc) / (float) 16.4;
			gyro_y_act = (float) (gyro_y_adc) / (float) 16.4;
			gyro_z_act = (float) (gyro_z_adc) / (float) 16.4;
			accel_x_act = (float) (accel_x_adc) / (float) 2048;
			accel_y_act = (float) (accel_y_adc) / (float) 2048;
			accel_z_act = (float) (accel_z_adc) / (float) 2048;
			temp_act = ((float) (temp_adc) - 25) / (float) 326.8 + 25;

			/* 原始值 */
			printf("Original:\tgx = %d, gy = %d, gz = %d \t", gyro_x_adc, gyro_y_adc, gyro_z_adc);
			printf("ax = %d, ay = %d, az = %d \t", accel_x_adc, accel_y_adc, accel_z_adc);
			printf("temp = %d\r\n", temp_adc);

			/* 实际值 */
			printf("Actual: \tgx = %.2f°/S, gy = %.2f°/S, gz = %.2f°/S \t",
			       gyro_x_act, gyro_y_act, gyro_z_act);
			printf("ax = %.2fg, ay = %.2fg, az = %.2fg \t",
			       accel_x_act, accel_y_act, accel_z_act);
			printf("temp = %.2f°C\r\n", temp_act);
		}
		usleep(100000); /*100ms */
	}
	close(fd);    /* 关闭文件 */
	return 0;
}

