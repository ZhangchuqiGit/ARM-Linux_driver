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

#define LEDDEV_CNT       1           	/* 设备号个数 	*/
#define LEDDEV_NAME      "zcq_dtsplatled"	/* 设备名字 	*/
#define LEDOFF           0            	/* 关灯 */
#define LEDON            1            	/* 开灯 */

/* 寄存器物理地址：设备树方式不需要 */

/* 映射后的寄存器虚拟地址指针：princtl 方式不需要 */

/* leddev设备结构体 */
struct leddev_dev {
	dev_t devid;                /* 设备号	*/
	struct cdev cdev;            /* cdev		*/
	struct class *class;        /* 类 		*/
	struct device *device;        /* 设备		*/
	int major;                    /* 主设备号	*/
	struct device_node *node;    /* LED设备节点 */
	int led_gpio;                // led 所使用的 Gled_switch PIO 编号
};

struct leddev_dev leddev;        /* led设备 */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &leddev; /* 设置私有数据  */
	return 0;
}

/*
 * @description		: 从设备读取数据
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	/* 	copy_to_user() 将数据从内核空间拷贝到用户空间,一般是在驱动中 chr_drv_read()用
参数1：内核驱动中的一个buffer
参数2：应用空间到一个buffer
参数3：个数   */

	return 0;
}

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void led_switch(u8 sta)
{
	if (sta == LEDON)
		/* copy_from_user(): 将数据从用户空间拷贝到内核空间,一般是在驱动中 chr_drv_write()用
参数1：应用驱动中的一个buffer
参数2：内核空间到一个buffer
参数3：个数
返回值：大于0，表示出错，剩下多少个没有拷贝成功; 等于0，表示正确  */
		gpio_set_value(leddev.led_gpio, 0);  /* 打开LED灯
		gpio 子系统常用的 API 函数: 此函数用于设置某个 GPIO 的值 */
	else if (sta == LEDOFF)
		gpio_set_value(leddev.led_gpio, 1);  /* 关闭LED灯 */
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[2];
	unsigned char ledstat;

	retvalue = copy_from_user(databuf, buf, cnt);
	if (retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];
	led_switch(ledstat); // LED打开/关闭
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 设备操作函数 */
static struct file_operations led_fops = {
		.owner = THIS_MODULE,
		.open = led_open,
		.write = led_write,
/*        .read = led_read, // 此处无用 */
		.release = led_release,
};

/*
 * @description		: flatform驱动的probe函数，当驱动与设备匹配以后此函数就会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int led_probe(struct platform_device *dev)
{
	int ret = 0;

	/* 获取设备树中的属性数据 设置led所使用的GPIO */
	/* 1、获取设备节点：gpioled */
	/** 若驱动程序采用 设备树方式，设备树 接口： /sys/devices/platform/gpioled */
	leddev.node = of_find_node_by_path("/gpioled");
	if (leddev.node == NULL) {
		printk("gpioled node nost find!\r\n");
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到LED所使用的LED编号 */
	leddev.led_gpio = of_get_named_gpio(leddev.node, "led-gpio", 0);
	if (leddev.led_gpio < 0) {
		printk("can't get led-gpio\r\n");
		return -EINVAL;
	}

	gpio_request(leddev.led_gpio, "led_gpio");//请求 IO: 申请一个 GPIO 管脚

	/* 3、设置GPIO1_IO03为输出，并且输出高电平，默认关闭LED灯 */
	ret = gpio_direction_output(leddev.led_gpio, 1); /* led_gpio IO设置为输出，默认高电平	*/
	if (ret < 0) {
		printk("can't set gpio!\r\n");
	}



	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (leddev.major) {  /*  定义了设备号 */
		leddev.devid = MKDEV(leddev.major, 0);
		register_chrdev_region(leddev.devid, LEDDEV_CNT, LEDDEV_NAME/*设备名字*/);
	} else {            /* 没有定义设备号 */
		alloc_chrdev_region(&leddev.devid, 0, LEDDEV_CNT, LEDDEV_NAME);/* 申请设备号 */
		leddev.major = MAJOR(leddev.devid);/* 获取分配号的主设备号 */
	}

	/* 2、初始化cdev */
	cdev_init(&leddev.cdev, &led_fops);

	/* 3、添加一个cdev */
	cdev_add(&leddev.cdev, leddev.devid, LEDDEV_CNT);

	/* 4、创建类      */
	leddev.class = class_create(THIS_MODULE, LEDDEV_NAME);
	if (IS_ERR(leddev.class)) {
		return PTR_ERR(leddev.class);
	}

	/* 5、创建设备 */
	leddev.device = device_create(leddev.class, NULL, leddev.devid, NULL,
								  LEDDEV_NAME);
	if (IS_ERR(leddev.device)) {
		return PTR_ERR(leddev.device);
	}
	return 0;
}

/* @description		: platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int led_remove(struct platform_device *dev)
{
	gpio_set_value(leddev.led_gpio, 1);    /* 卸载驱动的时候关闭LED */

	/* 注销字符设备驱动 */
	cdev_del(&leddev.cdev);                /*  删除cdev */
	unregister_chrdev_region(leddev.devid, LEDDEV_CNT); /* 注销设备号 */

	device_destroy(leddev.class, leddev.devid);
	class_destroy(leddev.class);
	return 0;
}

/* 匹配列表 */
static const struct of_device_id led_of_match[] = {
		{.compatible = "atkalpha-gpioled"}, /* 兼容属性 */
		{ /* Sentinel */ }
};
/* platform驱动结构体 */
static struct platform_driver led_driver = {
		.driver        = {
				.name    = "imx6ul-led",            /* 驱动名字，用于和设备匹配 */
				.of_match_table    = led_of_match, /* 设备树匹配表 */
		},
		.probe        = led_probe,
		.remove        = led_remove,
};

/*
 * @description	: 驱动模块加载函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init

leddriver_init(void)
{
	return platform_driver_register(&led_driver);
}

/*
 * @description	: 驱动模块卸载函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit leddriver_exit(void)
{
	platform_driver_unregister(&led_driver);
}

module_init(leddriver_init);
module_exit(leddriver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");



