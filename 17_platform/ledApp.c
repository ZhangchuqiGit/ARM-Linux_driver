#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

/** 在没有设备树的 Linux 内核下，我们需要分别编写并注册 platform_device 和 platform_driver，
 * 分别代表设备和驱动。在使用设备树的时候，设备的描述被放到了设备树中，
 * 因此 platform_device 就不需要我们去编写了，我们只需要实现 platform_driver 即可。 */

#define LEDOFF 	0
#define LEDON 	1

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd, retvalue;
	char *filename;
	unsigned char databuf[2];
	
	if(argc != 3){
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];

	/* 打开led驱动 */
	fd = open(filename, O_RDWR);
	if(fd < 0){
		printf("file %s open failed!\r\n", argv[1]);
		return -1;
	}
	
	databuf[0] = atoi(argv[2]);	/* 要执行的操作：打开或关闭 */
	retvalue = write(fd, databuf, sizeof(databuf));
	if(retvalue < 0){
		printf("LED Control Failed!\r\n");
		close(fd);
		return -1;
	}

	retvalue = close(fd); /* 关闭文件 */
	if(retvalue < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}
	return 0;
}
