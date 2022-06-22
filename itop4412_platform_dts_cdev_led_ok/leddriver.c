
/** Linux 中的 class 是设备类， 它是一个抽象的概念， 没有对应的实体。
 * 它是提供给用户接口相似的一类设备的集合。
 * 常见的有输入子系统 input、 usb、 串口 tty、 块设备 block 等。
 * 以 4412 的串口为例， 它有四个串口， 不可能为每一个串口都重复申请设备以及设备节点， 
 * 因为它们有类似的地方， 而且很多代码都是重复的地方， 所以引入了一个抽象的类， 
 * 将其打包为 ttySACX， 在实际调用串口的时候， 只需要修改 X 值，就可以调用不同的串口。 */

/** 在没有设备树的 Linux 内核下，
 * 我们需要分别编写并注册 platform_device 和 platform_driver，
 * 分别代表设备和驱动。
 * 在使用设备树的时候，设备的描述被放到了设备树中 *.dts，
 * 因此 platform_device 就不需要我们去编写了，我们只需要实现 platform_driver 即可。 */

#include <linux/init.h>

/*包含初始化宏定义的头文件,代码中的 module_init 和 module_exit, 
包含初始化加载模块的头文件, MODULE_LICENSE 在此文件中*/
#include <linux/module.h>

/*定义 module_param module_param_array 中 perm 的头文件*/
#include <linux/moduleparam.h>

/*platform 驱动注册的头文件，包含驱动的结构体和注册和卸载的函数*/
#include <linux/platform_device.h>

/*MISC 杂项设备*/
#include <linux/miscdevice.h>

/*注册设备节点的文件结构体;三个字符设备函数*/
#include <linux/fs.h>

/*MKDEV 转换设备号 数据类型的宏定义*/
#include <linux/kdev_t.h>

/*定义 字符设备 的结构体*/
#include <linux/cdev.h>

/*Linux中申请GPIO的头文件*/
#include <linux/gpio.h>

/*分配内存空间函数头文件*/
#include <linux/slab.h>

/* 设备类 */
#include <linux/device.h>

#include <linux/types.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/err.h>

/********************************************************************/

#define DRIVER_NAME "zcq_led" /* 驱动名字 */

#define DEV_MAJOR 134 /* 定义主设备号 */
#define DEV_MINOR 2	  /* 定义次设备号 */

#define LEDDEV_CNT         2	 /* 设备号个数 / LED灯的个数 */
#define LEDDEV_NAME "zcq_dtsplatled" /* 设备名字 /dev/zcq_dtsplatled	*/

#define LEDOFF 0 /* 关灯 */
#define LEDON 1	 /* 开灯 */

/* 寄存器物理地址：设备树方式不需要 */

/* 映射后的寄存器虚拟地址指针：princtl 方式不需要 */

/** *************** 设备操作 ********************* **/

/* leddev设备结构体 */
struct leddev_dev
{
	dev_t devid;			  /* 设备号	*/
	struct cdev cdev;		  /* 字符设备	*/
	struct class *class;	  /* 类 		*/
	struct device *device;	  /* 设备		*/
	int major;				  /* 主设备号	*/
	int minor;				  /* 次设备号   */
	struct device_node *node; /* 用于获取设备树节点 */
	//int led_gpio;			// GPIO 编号
	int led_gpio[LEDDEV_CNT]; //用于获取 LEDDEV_CNT 个 led 的GPIO编号
};
struct leddev_dev leddev; /* led设备 */

/********************************************************************/

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
	/*在设备操作集中，我们尽量使用私有数据来操作对象*/
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
	int i=0;
	if (sta == LEDON)
		/* copy_from_user(): 将数据从用户空间拷贝到内核空间,一般是在驱动中 chr_drv_write()用
参数1：应用驱动中的一个buffer
参数2：内核空间到一个buffer
参数3：个数
返回值：大于0，表示出错，剩下多少个没有拷贝成功; 等于0，表示正确  */
		for (i = 0; i < LEDDEV_CNT; i++)
			gpio_set_value(leddev.led_gpio[i], 1); /* 打开LED灯
			gpio 子系统常用的 API 函数: 此函数用于设置某个 GPIO 的值 */
	else if (sta == LEDOFF)
		for (i = 0; i < LEDDEV_CNT; i++)
			gpio_set_value(leddev.led_gpio[i], 0); /* 关闭LED灯 */
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
	struct leddev_dev *_led_dev = filp->private_data;	//获取私有变量

	retvalue = copy_from_user(databuf, buf, cnt);
	if (retvalue < 0)
	{
		printk("kernel write failed!\r\n");
		return -EINVAL;
	}

	ledstat = databuf[0]; /* 获取状态值 */
	led_switch(ledstat);  // LED打开/关闭
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

/********************************************************************/

/* 设备操作结构体 */
static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.write = led_write,
	// .read = led_read, // 此处无用
	.release = led_release,
};

/** *************** platform 驱动 结构体 函数 ********************* **/

int leds_init(struct platform_device *dev) // 初始化led的GPIO
{
	int ret = 0;
	int i = 0;
	char led_labels[][20] =
		{"led_gpio_0", "led_gpio_1"}; //定义设备标签

	/* 获取设备树中的属性数据 设置led所使用的GPIO */
	/* 1、获取设备节点：gpioled */
#if 1
	/** 若驱动程序采用 设备树 + platform 方式, 接口： /sys/devices/platform/zcq_beep */
	/*通过节点路径来查找设备树节点，若查找失败，则返回NULL*/
	leddev.node = of_find_node_by_path("/zcq_led"); /*设备节点*/
#else
	miscbeep.node = pdev->dev.of_node; /*设备节点*/
#endif
	if (leddev.node == NULL)
	{
		printk("/zcq_led node nost find!\r\n");
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到所使用的编号 */
	/*通过设备树节点、属性名、属性索引号来获取GPIO编号，若获取失败，则返回一个负数*/
	for (i = 0; i < LEDDEV_CNT; i++) {
		leddev.led_gpio[i] = of_get_named_gpio(leddev.node, "led-gpios", i);
		if (leddev.led_gpio[i] < 0)
		{
			printk("can't get led-gpios %d\r\n", i);
			ret = -EINVAL;
			goto fail_get_gpio;
		}
		else //打印出获取的gpio编号
			printk("led_gpio_%d = %d\r\n", i, leddev.led_gpio[i]);
	}

	/* 3、申请 gpio */
	for (i = 0; i < LEDDEV_CNT; i++) {
		//请求 IO: 申请一个 GPIO 管脚
		ret = gpio_request(leddev.led_gpio[i], led_labels[i]);
		if (ret) //如果申请失败
		{
			printk("can't request the led_gpio_%d\r\n", i);
			ret = -EINVAL;
			goto fail_request_gpio;
		}
	}

	/* 4、设置为输出/输入 */
	for (i = 0; i < LEDDEV_CNT; i++)
	{
		ret = gpio_direction_output(leddev.led_gpio[i], 1);
		if (ret < 0)
		{
			printk("failed to set led_gpio_%d\r\n", i);
			ret = -EINVAL;
			goto fail_set_output;
		}
	}

	/*5.设置输出高电平*/
	for (i = 0; i < LEDDEV_CNT; i++)
		gpio_set_value(leddev.led_gpio[i], 1); /* 关闭LED灯 */

	return 0;

fail_set_output:
fail_request_gpio:
fail_get_gpio:
	for (i = 0; i < LEDDEV_CNT; i++) {
		gpio_set_value(leddev.led_gpio[i], 0); 	/* 卸载驱动的时候关闭LED */
		gpio_free(leddev.led_gpio[i]);/*释放已经被占用的IO*/
	}
	return ret;
}

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int led_probe(struct platform_device *dev)
{
	if (leds_init(dev)) return -1; // 初始化led的GPIO

	leddev.major=0; 
	leddev.minor=0; 
#ifdef DEV_MAJOR 				/** 定义主设备号 */
	leddev.major=DEV_MAJOR;		/* 主设备号 */
	#ifdef DEV_MINOR
		leddev.minor=DEV_MINOR; /* 次设备号 */
	#endif
#endif

	/** 注册字符设备驱动 */
	/* 1、创建设备号 */
	/*	MAJOR(dev)， 就是对 dev 操作， 提取高 12 位主设备号 ；
		MINOR(dev) ， 就是对 dev 操作， 提取低 20 位数次设备号 ；
		MKDEV(ma,mi) ， 就是对主设备号和次设备号操作， 合并为 dev 类型。*/
		if (leddev.major) { 		/**  定义了设备号, 静态申请设备号 */
		leddev.devid = MKDEV(leddev.major, leddev.minor);
		register_chrdev_region(leddev.devid, LEDDEV_CNT, LEDDEV_NAME /*设备名字*/);
		printk("静态申请设备号\tmajor=%d\tminor=%d\r\n", 
			leddev.major, leddev.minor );
	} 
	else { 								/** 没有定义设备号, 动态申请设备号 */
		alloc_chrdev_region(&leddev.devid, leddev.minor, LEDDEV_CNT, LEDDEV_NAME /*设备名字*/);
		leddev.major = MAJOR(leddev.devid); /* 获取分配号的主设备号 */
		leddev.minor = MINOR(leddev.devid); /* 获取分配号的次设备号 */
		printk("动态申请设备号\tmajor=%d\tminor=%d\r\n", 
			leddev.major, leddev.minor );
	}

	/* 2、初始化 cdev */
	cdev_init(&leddev.cdev, &led_fops);

	/* 3、注册字符设备; 添加一个cdev*/
	cdev_add(&leddev.cdev, leddev.devid, LEDDEV_CNT);

	/* 4、创建类      */
	leddev.class = class_create(THIS_MODULE, LEDDEV_NAME);
	if (IS_ERR(leddev.class))
	{
		return PTR_ERR(leddev.class);
	}

	/* 5、创建设备 */
	leddev.device = device_create(leddev.class, NULL, leddev.devid, NULL,
								  LEDDEV_NAME);
	if (IS_ERR(leddev.device))
	{
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
	int i=0;
	for (i = 0; i < LEDDEV_CNT; i++) {
		gpio_set_value(leddev.led_gpio[i], 1);  /* 卸载驱动的时候关闭LED */
		gpio_free(leddev.led_gpio[i]); /*释放已经被占用的IO*/
	}

	/** 注销字符设备驱动 */
	cdev_del(&leddev.cdev);		/*卸载字符设备*/
	device_destroy(leddev.class, leddev.devid);/*摧毁设备*/
	class_destroy(leddev.class);/*摧毁类*/

	unregister_chrdev_region(leddev.devid, LEDDEV_CNT); /* 注销设备号 */
	return 0;
}

/** *************** 设备树 *.dts 匹配列表 ********************* **/

static const struct of_device_id led_of_match[] = {
	{.compatible = "zcq_gpioled"}, /* 兼容属性 */
    { /* sentinel */ }
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
// MODULE_DEVICE_TABLE(of, led_of_match);

/** *************** platform 驱动 结构体 ********************* **/

static struct platform_driver led_driver = {
	.probe = led_probe,	  /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.remove = led_remove, /*移除驱动*/
	// .shutdown = hello_shutdown,	/*关闭驱动*/
	// .suspend = hello_suspend, 		/*悬挂（休眠）驱动*/
	// .resume = hello_resume,		/*驱动恢复后要做什么*/
	.driver = {
		.name = DRIVER_NAME, 	/* 驱动名字 */
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(led_of_match), /* 设备树匹配表 */
	},
};

/********************************************************************/

/*驱动模块加载函数*/
static int __init leddriver_init(void)
{
	printk(KERN_EMERG "leddriver_init\n");
	return platform_driver_register(&led_driver); /*向系统注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit leddriver_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "leddriver_exit\n");
	platform_driver_unregister(&led_driver); /*向系统注销驱动*/
}

/********************************************************************/

module_init(leddriver_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(leddriver_exit); /*卸载驱动时运行的函数， 如 rmmod*/

MODULE_LICENSE("Dual BSD/GPL"); /*Dual BSD/GPL 声明是开源的，没有内核版本限制*/
MODULE_AUTHOR("zcq");			/*声明作者*/
MODULE_DESCRIPTION("zcq beep");
