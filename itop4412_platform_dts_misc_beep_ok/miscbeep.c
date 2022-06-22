
/**
 互斥锁：是为上锁而优化；
 条件变量：是为等待而优化；
 信号量：既可上锁，也可等待，故开销大于前二者。
 **/

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

/** 杂项设备的主设备号已经固定为‘10’。
有时要考虑到自己申请主设备号以及次设备号， 就是标准的字符设备 */ 

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

/* 设备类 */
#include <linux/device.h>

#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/delay.h>

/********************************************************************/

#define DRIVER_NAME "zcq_beep_driver" 		/* 驱动名字 */
#define MISC_DEVICE_NAME "zcq_beep_device" 	/* MISC 设备名字 /dev/zcq_beep_device */

#define MISCBEEP_MINOR 156 /* 子设备号 */

#define BEEPOFF 0 /* 关蜂鸣器 */
#define BEEPON 1  /* 开蜂鸣器 */

/* 寄存器物理地址：设备树方式不需要 */

/* 映射后的寄存器虚拟地址指针： princtl 方式不需要 */

/** *************** 设备操作 ********************* **/

/* miscbeep设备结构体 */
struct miscbeep_dev
{
	dev_t devid;			  /* 设备号 	 */
	struct cdev cdev;		  /* cdev 	*/
	struct class *class;	  /* 类 		*/
	struct device *device;	  /* 设备 	 */
	struct device_node *node; /* 设备节点 */
	uint32_t beep_gpio;		  // led 所使用的 Gled_switch PIO 编号
};
struct miscbeep_dev miscbeep; /* beep设备 */

/********************************************************************/

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

	/* copy_from_user(): 将数据从用户空间拷贝到内核空间,一般是在驱动中 chr_drv_write()用
参数1：应用驱动中的一个buffer
参数2：内核空间到一个buffer
参数3：个数
返回值：大于0，表示出错，剩下多少个没有拷贝成功; 等于0，表示正确  */
	retvalue = copy_from_user(databuf, buf, cnt);
	if (retvalue < 0)
	{
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	beepstat = databuf[0]; /* 获取状态值 */
	if (beepstat == BEEPON)
	{
		gpio_set_value(dev->beep_gpio, 1); /* 打开蜂鸣器
 		gpio 子系统常用的 API 函数: 此函数用于设置某个 GPIO 的值 */
	}
	else if (beepstat == BEEPOFF)
	{
		gpio_set_value(dev->beep_gpio, 0); /* 关闭蜂鸣器 */
	}
	return cnt;
}

/********************************************************************/

/* 设备操作结构体 */
static struct file_operations miscbeep_fops = {
	.owner = THIS_MODULE,		/*表示本模块拥有*/
	.open = miscbeep_open,
	.write = miscbeep_write,
	// .read = miscbeep_read, // 此处无用 
	// .release = miscbeep_release, // 此处无用 
};

/* MISC 设备结构体 */
static struct miscdevice beep_miscdev = {
	.minor = MISCBEEP_MINOR, 	// 子设备号
	.name = MISC_DEVICE_NAME, 	// 设备名字
	.fops = &miscbeep_fops,
};

/** *************** platform 驱动 结构体 函数 ********************* **/

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int miscbeep_probe(struct platform_device *pdev)
{
	int ret = 0;

	/* 获取设备树中的属性数据 设置所使用的GPIO */
	/* 1、获取设备节点 */
#if 1
	/** 若驱动程序采用 设备树 + platform 方式, 接口： /sys/devices/platform/zcq_beep */
	miscbeep.node = of_find_node_by_path("/zcq_beep"); /*设备节点*/
#else
	miscbeep.node = pdev->dev.of_node; /*设备节点*/
#endif
	if (miscbeep.node == NULL)
	{
		printk("zcq_beep node not find!\r\n");
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到所使用的编号 */
	miscbeep.beep_gpio = of_get_named_gpio(miscbeep.node, "zcq_beep-gpios", 0);
	if (miscbeep.beep_gpio == -EPROBE_DEFER) return miscbeep.beep_gpio;
	if (miscbeep.beep_gpio < 0) {
		dev_err(&pdev->dev, "error acquiring zcq_beep gpio: %d\n", 
			miscbeep.beep_gpio);
		return miscbeep.beep_gpio;
	}

	/* 3、申请 gpio */
	ret = devm_gpio_request_one(&pdev->dev, miscbeep.beep_gpio, 0, "relay-gpio");
	if(ret) {
			dev_err(&pdev->dev, "error requesting zcq_beep gpio: %d\n", ret);
			return ret;
	}

	/* 4、设置为输出 */
	ret = gpio_direction_output(miscbeep.beep_gpio, 0);
	if (ret < 0) {
		printk("can't set gpio!\r\n");
	}

	/** 一般情况下会注册对应的字符设备 cdev，但是这里我们使用 MISC 设备，
	 * 所以我们不需要自己注册字符设备驱动，只需要注册misc设备驱动即可	 */
	ret = misc_register(&beep_miscdev); /*注册 misc 杂项设备*/
	if (ret < 0) {
		printk("misc device register failed!\r\n");
		return -EFAULT;
	}

	printk("miscbeep_probe()\n");
	return 0;
}

/*移除驱动*/
static int miscbeep_remove(struct platform_device *dev)
{
	gpio_set_value(miscbeep.beep_gpio, 0); /* 卸载驱动的时候关闭LED */
	misc_deregister(&beep_miscdev);		   /*注销 misc 杂项设备*/
	devm_gpio_free(miscbeep.beep_gpio); /*释放已经被占用的 IO*/
	return 0;
}

/** *************** 设备树 *.dts 匹配列表 ********************* **/

static const struct of_device_id beep_of_match[] = {
	{.compatible = "zcq_beeper"}, /* 兼容属性 */
	{},
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE(of, beep_of_match);

/** *************** platform 驱动 结构体 ********************* **/

static struct platform_driver beep_driver = {
	.probe = miscbeep_probe,   /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.remove = miscbeep_remove, /*移除驱动*/
	// .shutdown = hello_shutdown,			/*关闭驱动*/
	// .suspend = hello_suspend,			/*悬挂（休眠）驱动*/
	// .resume = hello_resume,				/*驱动恢复后要做什么*/
	.driver = {
		.name = DRIVER_NAME, /* 名字，用于驱动和设备的匹配 */
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(beep_of_match), /* 设备树匹配表 */
	}
};

/********************************************************************/
#if 0
/*驱动模块自动加载、卸载函数*/
module_platform_driver(beep_driver);
#else
/*驱动模块加载函数*/
static int __init miscbeep_init(void)
{
	printk(KERN_EMERG "%s()\n", __func__);
	return platform_driver_register(&beep_driver); /*向系统注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit miscbeep_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s()\n", __func__);
	platform_driver_unregister(&beep_driver); /*向系统注销驱动*/
}

module_init(miscbeep_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(miscbeep_exit); /*卸载驱动时运行的函数， 如 rmmod*/
#endif
/********************************************************************/

MODULE_LICENSE("GPL"); 	/* 声明开源，没有内核版本限制*/
MODULE_AUTHOR("zcq");/*声明作者*/
MODULE_DESCRIPTION("zcq beep");
