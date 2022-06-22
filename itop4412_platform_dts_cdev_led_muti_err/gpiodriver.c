
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

#define DRIVER_NAME "zcq_led_driver" /* 驱动名字 */

// #define REGDEV_SIZE  		1024

#define DEVICE_NUM     	2 						/* 设备号个数  */
#define DEVICE_NAME 	"zcq_led_device" 	/* 设备名字 */
/* ls /dev/zcq_led_device?
使用命令“ ls /sys/class/zcq_led_device/ ” 可以查看到生成的 class
zcq_led_device0  zcq_led_device1 ... zcq_led_device?
 也是可以通过命令来创建设备节点的， 
 使用命令“ mknod dev/test0 c 249 0 ”和“ mknod dev/test1 c 249 1 ”    */

#define DEV_MAJOR 0 	/* 定义主设备号; 0=动态申请设备号 */
#define DEV_MINOR 0	  	/* 定义次设备号 */

#ifdef DEV_MAJOR 				
	int numdev_major = DEV_MAJOR;	 		/* 主设备号 */
	/** 带参数的加载驱动 insmod  ptrdriver.ko numdev_major=223 numdev_minor=12 */	
	module_param(numdev_major, int, S_IRUSR);	/*输入主设备号*/
#endif
#ifdef DEV_MINOR
	int numdev_minor = DEV_MINOR;		/* 次设备号 */
	/** 带参数的加载驱动 insmod  ptrdriver.ko numdev_major=223 numdev_minor=12 */	
	module_param(numdev_minor, int, S_IRUSR);		/*输入次设备号*/
#endif

/** *************** 设备操作 ********************* **/

/* 设备结构体 */
struct led_dev_struct
{
	// char *data;
	struct cdev cdev;		  /* 字符设备	*/
	struct device *device;	  /* 设备		*/
	int gpio;			// GPIO 编号
};
struct led_dev_struct *led_dev_sptr;

struct device_node *mynode_led; /* 用于获取设备树节点 */
struct class *myclass_led;	  /* 类 		*/

/********************************************************************/

/* 打开操作 */
static int file_open(struct inode *inode, struct file *filp) {
	/*在设备操作集中，我们尽量使用私有数据来操作对象*/
	filp->private_data = led_dev_sptr; /* 设置私有数据  */
	return 0;
}

/* 从设备读取数据 */
static ssize_t file_read(struct file *filp, char __user *buf, size_t count, loff_t *f_ops) {
	/* 	copy_to_user() 将数据从内核空间拷贝到用户空间,一般是在驱动中 chr_drv_read()用
	参数1：内核驱动中的一个buffer
	参数2：应用空间到一个buffer
	参数3：个数   */
	return 0;
}

/* LED打开/关闭 */
void led_switch(u8 sta) {
	int i=0;
	if (sta == 1) {
		for (i = 0; i < DEVICE_NUM; i++)
			gpio_set_value(led_dev_sptr[i].gpio, 1); /* 打开LED灯 */
	}
	else if (sta == 0) {
		for (i = 0; i < DEVICE_NUM; i++)
			gpio_set_value(led_dev_sptr[i].gpio, 0); /* 关闭LED灯 */
	}
}

/* 向设备写数据 */
static ssize_t file_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_ops) {
	struct led_dev_struct *_dev_sptr = filp->private_data;	//获取私有变量
	int ret = 0;
	char databuf[2];

	/* copy_from_user(): 将数据从用户空间拷贝到内核空间,一般是在驱动中 chr_drv_write()用
	参数1：应用驱动中的一个buffer
	参数2：内核空间到一个buffer
	参数3：个数
	返回值：大于0，表示出错，剩下多少个没有拷贝成功; 等于0，表示正确  */
	ret = copy_from_user(databuf, buf, count);
	if (ret < 0) {
		printk("kernel write failed!\r\n");
		return -EINVAL;
	}

	led_switch(databuf[0]);  // LED打开/关闭
	return 0;
}

/*关闭/释放操作*/
static int file_release(struct inode *inode, struct file *filp) {
	return 0;
}

/*IO操作*/
static long file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	return 0;
}

loff_t file_llseek(struct file *filp, loff_t offset, int ence) {
	return 0;
}

/********************************************************************/

/* 设备操作结构体 */
static struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.open = file_open,
	.read = file_read,
	.write = file_write,
	.release = file_release,
	.unlocked_ioctl = file_ioctl,
	.llseek = file_llseek,
};

/********************************************************************/

int leds_init(struct platform_device *dev) // 初始化led的GPIO
{
	int ret = 0;
	int i = 0;
	char led_labels[][20] =
		{"led_gpio_0", "led_gpio_1"}; //定义设备标签

	/* 获取设备树中的属性数据 设置led所使用的GPIO */
	/* 1、获取设备节点：gpioled */
#if 1
	/** 若驱动程序采用 设备树 + platform 方式, 接口： /sys/devices/platform/zcq_led */
	/*通过节点路径来查找设备树节点，若查找失败，则返回NULL*/
	mynode_led = of_find_node_by_path("/zcq_led"); /*设备节点*/
#else
	miscbeep.node = pdev->dev.of_node; /*设备节点*/
#endif
	if (mynode_led == NULL) {
		printk("/zcq_led node nost find!\r\n");
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到所使用的编号 */
	/*通过设备树节点、属性名、属性索引号来获取GPIO编号，若获取失败，则返回一个负数*/
	for (i = 0; i < DEVICE_NUM; i++) {
		led_dev_sptr[i].gpio = of_get_named_gpio(mynode_led, "led-gpios", i);
		if (led_dev_sptr[i].gpio < 0) {
			printk("[%d] can't get led-gpios\r\n", i);
			ret = -EINVAL;
			goto fail_get_gpio;
		}
		else //打印出获取的gpio编号
			printk("%s = %d\r\n", led_labels[i], led_dev_sptr[i].gpio);

		/* 3、申请 gpio */
		//请求 IO: 申请一个 GPIO 管脚
		ret = gpio_request(led_dev_sptr[i].gpio, led_labels[i]);
		if (ret) //如果申请失败
		{
			printk("can't request the %s\r\n", led_labels[i]);
			ret = -EINVAL;
			goto fail_request_gpio;
		}

		/* 4、设置为输出/输入 */
		ret = gpio_direction_output(led_dev_sptr[i].gpio, 0);
		if (ret < 0) {
			printk("failed to set %s\r\n", led_labels[i]);
			ret = -EINVAL;
			goto fail_set_output;
		}

		/*5.设置输出高/低电平*/
		gpio_set_value(led_dev_sptr[i].gpio, 0); 
	}

	return 0;

fail_set_output:
fail_request_gpio:
fail_get_gpio:
	for (i = 0; i < DEVICE_NUM; i++) {
		gpio_set_value(led_dev_sptr[i].gpio, 0); 	/* 卸载驱动的时候关闭LED */
		gpio_free(led_dev_sptr[i].gpio);/*释放已经被占用的IO*/
	}

	return ret;
}

/*设备注册到系统*/
static void reg_init_cdev(struct led_dev_struct *dev, int index) {
	int err = 0;
	int devno = MKDEV(numdev_major, numdev_minor + index);

	/* 2、初始化 cdev */
	cdev_init(&dev->cdev,&my_fops);

	dev->cdev.owner = THIS_MODULE;
	/*dev->cdev.ops = &my_fops;*/

	/* 3、注册字符设备; 添加 1 个cdev */
	err = cdev_add(&dev->cdev,devno,1);
	if(err) printk(KERN_EMERG "cdev_add fail! index=%d\t err=%d\r\n", index, err);
}

/** *************** platform 驱动 结构体 函数 ********************* **/

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int led_probe(struct platform_device *dev)
{
	int ret = 0, i;
	dev_t dev_id = 0;			  		/* 设备号	*/

	if (leds_init(dev)) return -1; // 初始化led的GPIO

	/** 注册字符设备驱动 */
	/* 1、创建设备号 */
	/*	MAJOR(dev)， 就是对 dev 操作， 提取高 12 位主设备号 ；
		MINOR(dev) ， 就是对 dev 操作， 提取低 20 位数次设备号 ；
		MKDEV(ma,mi) ， 就是对主设备号和次设备号操作， 合并为 dev 类型。*/
#ifdef DEV_MAJOR
	if (numdev_major) { 				/**  定义了设备号, 静态申请设备号 */
		dev_id = MKDEV(numdev_major, numdev_minor);
		ret = register_chrdev_region(dev_id, DEVICE_NUM, DEVICE_NAME /*设备名字*/);
		printk("静态申请设备号\t numdev_major=%d\t numdev_minor=%d\r\n", 
			numdev_major, numdev_minor );
	} 
	else { 								/** 没有定义设备号, 动态申请设备号 */
		ret = alloc_chrdev_region(&dev_id, numdev_minor, DEVICE_NUM, DEVICE_NAME /*设备名字*/);
		numdev_major = MAJOR(dev_id); /* 获取分配号的主设备号 */
		numdev_minor = MINOR(dev_id); /* 获取分配号的次设备号 */
		printk("动态申请设备号\t numdev_major=%d\t numdev_minor=%d\r\n", 
			numdev_major, numdev_minor );
	}
	if(ret<0) printk(KERN_EMERG "region failed!\r\n");	
#endif

	led_dev_sptr = kmalloc(DEVICE_NUM * sizeof(struct led_dev_struct), GFP_KERNEL);
	if(!led_dev_sptr){
		ret = -ENOMEM;
		goto fail;
	}
	memset(led_dev_sptr,0,DEVICE_NUM * sizeof(struct led_dev_struct));

	/* 4、创建设备类      */
	myclass_led = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(myclass_led)) return PTR_ERR(myclass_led);

	/* 设备注册到系统 */
	for(i=0; i<DEVICE_NUM; i++){
		// led_dev_sptr[i].data = kmalloc(REGDEV_SIZE, GFP_KERNEL);
		// memset(led_dev_sptr[i].data, 0, REGDEV_SIZE);		
		reg_init_cdev(&led_dev_sptr[i], i);  /*设备注册到系统*/
		/* 5、创建设备 */
		led_dev_sptr[i].device = device_create(myclass_led, NULL, 
			MKDEV(numdev_major, numdev_minor + i), NULL, 
			"%s%d", DEVICE_NAME, i);
		if (IS_ERR(led_dev_sptr[i].device)) return PTR_ERR(led_dev_sptr[i].device);
	}

	return 0;

fail:
	/*注销设备号*/
	unregister_chrdev_region(dev_id, DEVICE_NUM);
	printk(KERN_EMERG "kmalloc is fail!\n");

	return ret;
}

/* @description		: platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int led_remove(struct platform_device *dev)
{
	int i=0;
	dev_t dev_id = MKDEV(numdev_major, numdev_minor); /* 设备号	*/

	printk(KERN_EMERG "%s()\n", __func__);

	/** 注销字符设备驱动 */
	for (i = 0; i < DEVICE_NUM; i++) {
		gpio_set_value(led_dev_sptr[i].gpio, 0); 	/* 卸载驱动的时候关闭LED */
		gpio_free(led_dev_sptr[i].gpio); 			/* 释放已经被占用的IO */

		cdev_del(&(led_dev_sptr[i].cdev));			/*卸载字符设备*/	
		device_destroy(myclass_led, MKDEV(numdev_major, numdev_minor + i));/*摧毁设备*/
	}
	class_destroy(myclass_led);			/*摧毁类*/
	kfree(led_dev_sptr);					/** 释放内存 **/

	unregister_chrdev_region(dev_id, DEVICE_NUM); /* 注销设备号 */
	return 0;
}

/** *************** 设备树 *.dts 匹配列表 ********************* **/

static const struct of_device_id led_of_match[] = {
	{.compatible = "zcq_gpioled"}, /* 兼容属性 */
	{},
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
// MODULE_DEVICE_TABLE(of, led_of_match);

/** *************** platform 驱动 结构体 ********************* **/

/* platform驱动结构体 */
static struct platform_driver led_driver = {
	.probe = led_probe,	  /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.remove = led_remove, /*移除驱动*/
	// .shutdown = hello_shutdown,	/*关闭驱动*/
	// .suspend = hello_suspend, 		/*悬挂（休眠）驱动*/
	// .resume = hello_resume,		/*驱动恢复后要做什么*/
	.driver = {
		.name = DRIVER_NAME, /* 名字，用于驱动和设备的匹配 */
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(led_of_match), /* 设备树匹配表 */
	},
};

/********************************************************************/
#if 0
/*驱动模块自动加载、卸载函数*/
module_platform_driver(led_driver);
#else
/*驱动模块加载函数*/
static int __init led_driver_init(void)
{
	printk(KERN_EMERG "%s()\n", __func__);
	return platform_driver_register(&led_driver); /*向系统注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit led_driver_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s()\n", __func__);
	platform_driver_unregister(&led_driver); /*向系统注销驱动*/
}
#endif
/********************************************************************/

module_init(led_driver_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(led_driver_exit); /*卸载驱动时运行的函数， 如 rmmod*/

MODULE_LICENSE("GPL"); 	/* 声明开源，没有内核版本限制*/
MODULE_AUTHOR("zcq"); 	/*声明作者*/
MODULE_DESCRIPTION("zcq driver led");
