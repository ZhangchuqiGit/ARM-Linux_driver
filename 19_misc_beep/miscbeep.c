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

#include <linux/miscdevice.h>

/** 在没有设备树的 Linux 内核下，我们需要分别编写并注册 platform_device 和 platform_driver，
 * 分别代表设备和驱动。在使用设备树的时候，设备的描述被放到了设备树中，
 * 因此 platform_device 就不需要我们去编写了，我们只需要实现 platform_driver 即可。 */

/** MISC 杂项驱动
 	所有的 MISC 设备驱动的主设备号都为 10(固定的)，不同的设备使用不同的从设备号。
	随着 Linux 字符设备驱动的不断增加，设备号变得越来越紧张，尤其是主设备号，
 MISC 设备驱动就用于解决此问题。MISC 设备会自动创建 cdev，不需要像我们以前那样手动创建，
 因此采用 MISC 设备驱动可以简化字符设备驱动的编写。
 	我们需要向 Linux 注册一个 miscdevice 设备，MISC 设备的主设备号为 10，这个是固定的，
 需要用户指定子设备号，Linux 系统已经预定义了一些 MISC 设备的子设备号，
 include/linux/miscdevice.h。
--------------------------------------------------------------------------------
 	以前我们需要自己调用一堆的函数去创建设备，比如
 传统的创建设备过程
1 alloc_chrdev_region(); 	// 申请设备号
2 cdev_init(); 				// 初始化 cdev
3 cdev_add(); 				// 添加 cdev
4 class_create(); 			// 创建类
5 device_create(); 			// 创建设备
 现在以直接使用 misc_register 一个函数来完成这些步骤。
 当卸载设备驱动模块的时候需要调用 misc_deregister 函数来注销掉 MISC 设备  */

#define MISCBEEP_NAME    "zcq_misc_beep"   /* 设备名字 */
#define MISCBEEP_MINOR   144          /* 子设备号 */

#define BEEPOFF          0            /* 关蜂鸣器 */
#define BEEPON           1            /* 开蜂鸣器 */

/* 寄存器物理地址：设备树方式不需要 */

/* 映射后的寄存器虚拟地址指针：princtl 方式不需要 */

/* miscbeep设备结构体 */
struct miscbeep_dev {
	dev_t devid;            /* 设备号 	 */
	struct cdev cdev;        /* cdev 	*/
	struct class *class;    /* 类 		*/
	struct device *device;    /* 设备 	 */
	struct device_node *node; /* 设备节点 */
	int beep_gpio;            // led 所使用的 Gled_switch PIO 编号
};

struct miscbeep_dev miscbeep;        /* beep设备 */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int miscbeep_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &miscbeep; /* 设置私有数据 */
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
static ssize_t miscbeep_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char beepstat;
	struct miscbeep_dev *dev = filp->private_data;

	retvalue = copy_from_user(databuf, buf, cnt);
	if (retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	beepstat = databuf[0];        /* 获取状态值 */
	if (beepstat == BEEPON) {
		/* copy_from_user(): 将数据从用户空间拷贝到内核空间,一般是在驱动中 chr_drv_write()用
参数1：应用驱动中的一个buffer
参数2：内核空间到一个buffer
参数3：个数
返回值：大于0，表示出错，剩下多少个没有拷贝成功; 等于0，表示正确  */
		gpio_set_value(dev->beep_gpio, 0);    /* 打开蜂鸣器
 		gpio 子系统常用的 API 函数: 此函数用于设置某个 GPIO 的值 */
	} else if (beepstat == BEEPOFF) {
		gpio_set_value(dev->beep_gpio, 1);    /* 关闭蜂鸣器 */
	}
	return 0;
}

/* 设备操作函数 */
static struct file_operations miscbeep_fops = {
		.owner = THIS_MODULE,
		.open = miscbeep_open,
		.write = miscbeep_write,
};

/* MISC 设备结构体 */
static struct miscdevice beep_miscdev = {
		.minor = MISCBEEP_MINOR, // 子设备号
		.name = MISCBEEP_NAME, // 设备名字
		.fops = &miscbeep_fops,
};

/*
 * @description     : flatform驱动的probe函数，当驱动与
 *                    设备匹配以后此函数就会执行
 * @param - dev     : platform设备
 * @return          : 0，成功;其他负值,失败
 */
static int miscbeep_probe(struct platform_device *dev)
{
	int ret = 0;

	/* 获取设备树中的属性数据 设置所使用的GPIO */
	/* 1、获取设备节点：beep */
	miscbeep.node = of_find_node_by_path("/beep");
	if (miscbeep.node == NULL) {
		printk("beep node not find!\r\n");
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到BEEP所使用的BEEP编号 */
	miscbeep.beep_gpio = of_get_named_gpio(miscbeep.node, "beep-gpio", 0);
	if (miscbeep.beep_gpio < 0) {
		printk("can't get beep-gpio");
		return -EINVAL;
	}

	/* 3、设置GPIO5_IO01为输出，并且输出高电平，默认关闭BEEP */
	ret = gpio_direction_output(miscbeep.beep_gpio, 1); /* led_gpio IO设置为输出，默认高电平	*/
	if (ret < 0) {
		printk("can't set gpio!\r\n");
	}



	/** 注册 misc 设备 */
	/** 一般情况下会注册对应的字符设备，但是这里我们使用 MISC 设备，
	 * 所以我们不需要自己注册字符设备驱动，只需要注册misc设备驱动即可	 */
	ret = misc_register(&beep_miscdev);
	if (ret < 0) {
		printk("misc device register failed!\r\n");
		return -EFAULT;
	}
	return 0;
}

/*
 * @description     : platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev     : platform设备
 * @return          : 0，成功;其他负值,失败
 */
static int miscbeep_remove(struct platform_device *dev)
{
	gpio_set_value(miscbeep.beep_gpio, 1); /* 卸载驱动的时候关闭LED */

	/* 注销 misc 设备 */
	misc_deregister(&beep_miscdev);
	return 0;
}

/* 设备树匹配列表 */
static const struct of_device_id beep_of_match[] = {
		{.compatible = "atkalpha-beep"}, /* 兼容属性 */
		{ /* Sentinel */ }
};
/* platform驱动结构体 */
static struct platform_driver beep_driver = {
		.driver     = {
				.name   = "imx6ul-beep",         /* 驱动名字，用于和设备匹配 */
				.of_match_table = beep_of_match, /* 设备树匹配表          */
		},
		.probe      = miscbeep_probe,
		.remove     = miscbeep_remove,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init

miscbeep_init(void)
{
	return platform_driver_register(&beep_driver);
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit miscbeep_exit(void)
{
	platform_driver_unregister(&beep_driver);
}

module_init(miscbeep_init);
module_exit(miscbeep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
