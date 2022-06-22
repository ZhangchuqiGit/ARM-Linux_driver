/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include <linux/of_gpio.h>

#define DEVICE_NAME	"power_ctl"
#define DRIVER_NAME 	"power_ctl"

/** power_ctrl.c 驱动，主要负责初始化背光， LVDS，以及触摸芯片的 I2C 通信使能。 */

uint32_t POWER_GPIO = 0;
uint32_t LVDS_GPIO = 0;
uint32_t TP_GPIO = 0;
uint32_t HDMI_GPIO = 0;

/********************************************************************/

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int power_open(struct inode *inode, struct file *filp) {
	return 0;
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t power_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	char str[20];
	memset(str, 0, 20);

	/* copy_from_user(): 将数据从用户空间拷贝到内核空间,一般是在驱动中 chr_drv_write()用
参数1：应用驱动中的一个buffer
参数2：内核空间到一个buffer
参数3：个数
返回值：大于0，表示出错，剩下多少个没有拷贝成功; 等于0，表示正确  */
	if(copy_from_user(str, buf, cnt)) {
		printk("copy_from_user() Error\n");
		return -EINVAL;
	}

	printk("power_write(): %s\r\n", str);
	return cnt;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int power_close(struct inode *inode, struct file *filp) {
	return 0;
}

static long power_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	printk("%s: cmd = %d\n", __FUNCTION__, cmd);
	switch(cmd) {
		case 0:
			gpio_set_value(POWER_GPIO, 0);
			break;
		case 1:
			gpio_set_value(POWER_GPIO, 1);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

/********************************************************************/

/* 设备操作结构体 */
static struct file_operations power_ops = {
	.owner			= THIS_MODULE,
	.open			= power_open,
	.write			= power_write,
	.release		= power_close, 
	.unlocked_ioctl = power_ioctl,
};

/* MISC 设备结构体 */
static struct miscdevice power_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,  	// 子设备号
	.name = DEVICE_NAME, 			// 设备名字
	.fops = &power_ops,
};

/** *************** platform 驱动 结构体 函数 ********************* **/

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int power_probe(struct platform_device *pdev)
{
	int ret;

	/* 1、获取设备节点 */
	struct device_node *np = pdev->dev.of_node;

	/* 2、 获取设备树中的gpio属性，得到BEEP所使用的BEEP编号 */
	POWER_GPIO = of_get_named_gpio(np, "gpios", 0);
	if (POWER_GPIO == -EPROBE_DEFER)
                return POWER_GPIO;
        if (POWER_GPIO < 0) {
                dev_err(&pdev->dev, "error acquiring power gpio: %d\n", POWER_GPIO);
                return POWER_GPIO;
        }

        ret = devm_gpio_request_one(&pdev->dev, POWER_GPIO, 0, "power-gpio");
        if(ret) {
                dev_err(&pdev->dev, "error requesting power gpio: %d\n", ret);
                return ret;
        }

	/* 3、设置GPIO5_IO01为输出，并且输出高电平，默认关闭BEEP */
	gpio_direction_output(POWER_GPIO, 1);

	
	LVDS_GPIO = of_get_named_gpio(np, "gpios", 1);
	if (LVDS_GPIO == -EPROBE_DEFER)
                return LVDS_GPIO;
        if (LVDS_GPIO < 0) {
                dev_err(&pdev->dev, "error acquiring power gpio: %d\n", LVDS_GPIO);
                return LVDS_GPIO;
        }

        ret = devm_gpio_request_one(&pdev->dev, LVDS_GPIO, 0, "power-gpio");
        if(ret) {
                dev_err(&pdev->dev, "error requesting power gpio: %d\n", ret);
                return ret;
        }

	gpio_direction_output(LVDS_GPIO, 1);


	TP_GPIO = of_get_named_gpio(np, "gpios", 2);
	if (TP_GPIO == -EPROBE_DEFER)
                return TP_GPIO;
        if (TP_GPIO < 0) {
                dev_err(&pdev->dev, "error acquiring power gpio: %d\n", TP_GPIO);
                return TP_GPIO;
        }

        ret = devm_gpio_request_one(&pdev->dev, TP_GPIO, 0, "power-gpio");
        if(ret) {
                dev_err(&pdev->dev, "error requesting power gpio: %d\n", ret);
                return ret;
        }
	
	gpio_direction_output(TP_GPIO, 1);

        HDMI_GPIO = of_get_named_gpio(np, "gpios", 3);
        if (HDMI_GPIO == -EPROBE_DEFER)
                return HDMI_GPIO;
        if (HDMI_GPIO < 0) {
                dev_err(&pdev->dev, "error acquiring power gpio: %d\n", TP_GPIO);
                return HDMI_GPIO;
        }

        ret = devm_gpio_request_one(&pdev->dev, HDMI_GPIO, 0, "power-gpio");
        if(ret) {
                dev_err(&pdev->dev, "error requesting power gpio: %d\n", ret);
                return ret;
        }

        gpio_direction_output(HDMI_GPIO, 1);


	ret = misc_register(&power_misc_dev);

	printk(DEVICE_NAME "\tinitialized\n");

	return 0;
}

/*移除驱动*/
static int power_remove (struct platform_device *pdev)
{
	gpio_free(POWER_GPIO);	/*释放已经被占用的 IO*/
	misc_deregister(&power_misc_dev); /*注销 misc 杂项设备*/
	return 0;
}

/*悬挂（休眠）驱动*/
static int power_suspend (struct platform_device *pdev, pm_message_t state)
{
	printk("power_ctl suspend:power off!\n");
	return 0;
}

/*驱动恢复后要做什么*/
static int power_resume (struct platform_device *pdev)
{
	printk("power_ctl resume:power on!\n");
	return 0;
}

/** *************** 设备树 *.dts 匹配列表 ********************* **/

#ifdef CONFIG_OF
static const struct of_device_id power_of_match[] = {
        { .compatible = "powerctrl-gpio" }, /* 兼容属性 */
        { /* sentinel */ }
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE(of, power_of_match);
#endif

/** *************** platform 驱动 结构体 ********************* **/

static struct platform_driver power_driver = {
	.probe = power_probe,	/*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.remove = power_remove, 	/*移除驱动*/
	// .shutdown = hello_shutdown,	/*关闭驱动*/
	.suspend = power_suspend,	/*悬挂（休眠）驱动*/
	.resume = power_resume,		/*驱动恢复后要做什么*/
	.driver = {
		.name = DRIVER_NAME, 	/* 驱动名字 */
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(power_of_match), /* 设备树匹配表 */
	},
};

/********************************************************************/

/*驱动模块加载函数*/
static int __init power_dev_init(void) {
	return platform_driver_register(&power_driver); /*向系统注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit power_dev_exit(void) {
	platform_driver_unregister(&power_driver); /*向系统注销驱动*/
}

/********************************************************************/

module_init(power_dev_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(power_dev_exit); /*卸载驱动时运行的函数， 如 rmmod*/

MODULE_LICENSE("GPL"); /*Dual BSD/GPL 声明是开源的，没有内核版本限制*/
MODULE_AUTHOR("zcq TOPEET");			/*声明作者*/
MODULE_DESCRIPTION("itop4412 power_ctrl.c driver");
