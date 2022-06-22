
/*
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
*/

/** 在没有设备树的 Linux 内核下，
 * 我们需要分别编写并注册 platform_device 和 platform_driver，
 * 分别代表设备和驱动。
 * 在使用设备树的时候，设备的描述被放到了设备树中，
 * 因此 platform_device 就不需要我们去编写了，我们只需要实现 platform_driver 即可。 */


#include <linux/init.h>
#include <linux/module.h>

/*platform 驱动注册的头文件，包含驱动的结构体和注册和卸载的函数*/
#include <linux/platform_device.h>

/********************************************************************/

#define DRIVER_NAME "hello_ctl" 	/* 名字，用于驱动和设备的匹配 */

/** *************** platform 驱动 结构体 函数 ********************* **/

/*匹配设备时加载驱动*/
static int hello_probe(struct platform_device *pdv) {
	printk(KERN_EMERG "\tinitialized\n");
	return 0;
}

/*移除驱动*/
static int hello_remove(struct platform_device *pdv) {
	return 0;
}

/*关闭驱动*/
static void hello_shutdown(struct platform_device *pdv) {
}

/*悬挂（休眠）驱动*/
static int hello_suspend(struct platform_device *pdv) {
	return 0;
}

/*驱动恢复后要做什么*/
static int hello_resume(struct platform_device *pdv) {
	return 0;
}

/** *************** platform 驱动 结构体 ********************* **/

struct platform_driver hello_driver = {
	.probe = hello_probe,				/*匹配设备时加载驱动*/
	.remove = hello_remove,				/*移除驱动*/
	.shutdown = hello_shutdown,			/*关闭驱动*/
	.suspend = hello_suspend,			/*悬挂（休眠）驱动*/
	.resume = hello_resume,				/*驱动恢复后要做什么*/
	.driver = {
		.name = DRIVER_NAME,  		/* 名字，用于驱动和设备的匹配 */
		.owner = THIS_MODULE,
	}
};

/********************************************************************/

/*驱动模块加载函数*/
static int __init hello_init(void) {
	int DriverState;
	printk(KERN_EMERG "HELLO WORLD enter!\n");
	DriverState = platform_driver_register(&hello_driver);/*向系统注册驱动*/
	printk(KERN_EMERG "DriverState is %d\n", DriverState);
	return 0;
}

/*驱动模块卸载函数*/
static void __exit hello_exit(void) {
	printk(KERN_EMERG "HELLO WORLD exit!\n");
	platform_driver_unregister(&hello_driver);/*向系统注销驱动*/
}

/********************************************************************/

module_init(hello_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(hello_exit); /*卸载驱动时运行的函数， 如 rmmod*/

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("zcq");
