#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
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

#include <linux/platform_device.h>

/** 在没有设备树的 Linux 内核下，我们需要分别编写并注册 platform_device 和 platform_driver，
 * 分别代表设备和驱动。在使用设备树的时候，设备的描述被放到了设备树中，
 * 因此 platform_device 就不需要我们去编写了，我们只需要实现 platform_driver 即可。 */

/* 寄存器物理地址 */
#define CCM_CCGR1_BASE                (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE        (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE        (0X020E02F4)
#define GPIO1_DR_BASE                (0X0209C000)
#define GPIO1_GDIR_BASE                (0X0209C004)
#define REGISTER_LENGTH                4

/** 映射后的寄存器虚拟地址指针 见 驱动 */

/* 设备资源信息，也就是LED0所使用的所有寄存器
 * 区分当前资源描述的是中断(IORESOURCE_IRQ)还是内存（IORESOURCE_MEM） */
static struct resource led_resources[] = {
		[0] = {
				.start    = CCM_CCGR1_BASE,
				.end    = (CCM_CCGR1_BASE + REGISTER_LENGTH - 1),
				.flags    = IORESOURCE_MEM,
		},
		[1] = {
				.start    = SW_MUX_GPIO1_IO03_BASE,
				.end    = (SW_MUX_GPIO1_IO03_BASE + REGISTER_LENGTH - 1),
				.flags    = IORESOURCE_MEM,
		},
		[2] = {
				.start    = SW_PAD_GPIO1_IO03_BASE,
				.end    = (SW_PAD_GPIO1_IO03_BASE + REGISTER_LENGTH - 1),
				.flags    = IORESOURCE_MEM,
		},
		[3] = {
				.start    = GPIO1_DR_BASE,
				.end    = (GPIO1_DR_BASE + REGISTER_LENGTH - 1),
				.flags    = IORESOURCE_MEM,
		},
		[4] = {
				.start    = GPIO1_GDIR_BASE,
				.end    = (GPIO1_GDIR_BASE + REGISTER_LENGTH - 1),
				.flags    = IORESOURCE_MEM,
		},
};

/* @description		: 释放flatform设备模块的时候此函数会执行
 * @param - dev 	: 要释放的设备
 * @return 			: 无
 */
static void led_release(struct device *dev)
{
	printk("led device released!\r\n");
}

/* platform设备结构体 */
static struct platform_device leddevice = {
		.name = "imx6ul-led", //用于做匹配
		.id = -1, // 一般都是直接给-1
		.dev = { // 继承了device父类
			.release = &led_release,
		},
		.num_resources = ARRAY_SIZE(led_resources), // 资源的个数
		.resource = led_resources, // 资源：包括了一个设备的地址和中断
};

/*
 * @description	: 设备模块加载 
 * @param 		: 无
 * @return 		: 无
 */
static int __init leddevice_init(void)
{
	return platform_device_register(&leddevice);
}

/*
 * @description	: 设备模块注销
 * @param 		: 无
 * @return 		: 无
 */
static void __exit leddevice_exit(void)
{
	platform_device_unregister(&leddevice);
}

module_init(leddevice_init);
module_exit(leddevice_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");





