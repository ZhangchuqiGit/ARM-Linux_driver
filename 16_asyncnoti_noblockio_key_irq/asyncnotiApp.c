#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "poll.h"
#include "sys/select.h"
#include "sys/time.h"
#include "linux/ioctl.h"
#include "signal.h"

/** 应用程序 对 异步通知 的处理包括以下 三步：
	1、注册信号处理函数
			应用程序根据驱动程序所使用的信号来设置信号的处理函数，
		应用程序使用 signal 函数来设置信号的处理函数。
	2、将本应用程序的进程号告诉给内核
		fcntl(fd, F_SETOWN, getpid()); // 设置当前进程接收SIGIO信号
	3、开启异步通知
			flags = fcntl(fd, F_GETFL); 		// 获取当前的进程状态
			fcntl(fd, F_SETFL, flags | FASYNC);	// 开启当前进程异步通知功能
		重点就是通过 fcntl 函数设置进程状态为 FASYNC，经过这一步，
		驱动程序中的 fasync 函数就会执行  */

static int fd = 0;	/* 文件描述符 */

/*
 * SIGIO信号处理函数
 * @param - signum 	: 信号值
 * @return 			: 无
 */
static void sigio_signal_func(int signum)
{
	int ret = 0;
	unsigned int keyvalue = 0;

	ret = read(fd, &keyvalue, sizeof(keyvalue));
	if(ret < 0) {
		/* 读取错误 */
	} else {
		printf("sigio signal! key value=%d\t\tret = %d\r\n", keyvalue, ret);
	}
}

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int flags = 0;
	char *filename;

	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if (fd < 0) {
		printf("Can't open file %s\r\n", filename);
		return -1;
	}

	/* 1、注册信号处理函数 */
	signal(SIGIO, sigio_signal_func);
	
	/* 2、将本应用程序的进程号告诉给内核 */
	fcntl(fd, F_SETOWN, getpid());		/* 设置当前进程接收 SIGIO 信号 	*/

	/* 3、开启异步通知 */
	flags = fcntl(fd, F_GETFL);			/* 获取当前的进程状态 			*/
	fcntl(fd, F_SETFL, flags | FASYNC);	/* 设置进程启用异步通知功能 	*/	

	while(1) {
		sleep(2);
	}

	close(fd);
	return 0;
}
