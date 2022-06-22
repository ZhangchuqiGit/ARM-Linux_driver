
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

#define DRIVER_NAME "zcq_reg_ptr_driver" /* 驱动名字 */

#define REGDEV_SIZE  		1024

#define DEVICE_NUM         2 			/* 设备号个数  */
#define DEVICE_NAME "zcq_reg_ptr_device" 	/* 设备名字 */
/* ls /dev/reg_ptr_device?
使用命令“ ls /sys/class/zcq_reg_ptr_device/ ” 可以查看到生成的 class
zcq_reg_ptr_device0  zcq_reg_ptr_device1 ... zcq_reg_ptr_device?
 也是可以通过命令来创建设备节点的， 
 使用命令“ mknod dev/test0 c 249 0 ”和“ mknod dev/test1 c 249 1 ”    */

#define DEV_MAJOR 0 	/* 定义主设备号; 0=动态申请设备号 */
#define DEV_MINOR 0	  	/* 定义次设备号 */

int numdev_major = DEV_MAJOR;	 	/* 主设备号 */
int numdev_minor = DEV_MINOR;		/* 次设备号 */
#if 1				
/** 带参数的加载驱动 insmod  ptrdriver.ko numdev_major=223 numdev_minor=12 */	
module_param(numdev_major, int, S_IRUSR);/*输入主设备号*/
module_param(numdev_minor, int, S_IRUSR);/*输入次设备号*/
#endif

/** *************** 设备操作 ********************* **/

/* zcqdev设备结构体 */
struct reg_dev
{
	char *data;
	struct cdev cdev;		  /* 字符设备	*/
	struct device *device;	  /* 设备		*/
	struct device_node *node; /* 用于获取设备树节点 */
	//int led_gpio;			// GPIO 编号
	//int led_gpio[DEVICE_NUM]; //用于获取 DEVICE_NUM 个GPIO编号
};
struct reg_dev *my_devices;

struct class *myclass;	  /* 类 		*/

/********************************************************************/

/* 打开操作 */
static int chardevnode_open(struct inode *inode, struct file *filp) {
	/*在设备操作集中，我们尽量使用私有数据来操作对象*/
	filp->private_data = my_devices; /* 设置私有数据  */
	return 0;
}

/* 从设备读取数据 */
static ssize_t chardevnode_read(struct file *filp, char __user *buf, size_t count, loff_t *f_ops) {
	return 0;
}

/* 向设备写数据 */
static ssize_t chardevnode_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_ops) {
	struct reg_dev *_reg_dev = filp->private_data;	//获取私有变量
	return 0;
}

/*关闭/释放操作*/
static int chardevnode_release(struct inode *inode, struct file *filp) {
	return 0;
}

/*IO操作*/
static long chardevnode_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	return 0;
}

loff_t chardevnode_llseek(struct file *filp, loff_t offset, int ence) {
	return 0;
}

/********************************************************************/

/* 设备操作结构体 */
static struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.open = chardevnode_open,
	.read = chardevnode_read,
	.write = chardevnode_write,
	.release = chardevnode_release,
	.unlocked_ioctl = chardevnode_ioctl,
	.llseek = chardevnode_llseek,
};

/********************************************************************/

/*设备注册到系统*/
static void reg_init_cdev(struct reg_dev *dev, int index) {
	int err = 0;
	int devno = MKDEV(numdev_major,numdev_minor+index);

	/* 2、初始化 cdev */
	cdev_init(&dev->cdev,&my_fops);

	dev->cdev.owner = THIS_MODULE;
	/*dev->cdev.ops = &my_fops;*/

	/* 3、注册字符设备; 添加一个cdev*/
	err = cdev_add(&dev->cdev,devno,1);
	if(err) printk(KERN_EMERG "cdev_add fail! index=%d\t err=%d\r\n", index, err);
}

/*驱动模块加载函数*/
static int __init regdriver_init(void)
{
	int ret = 0, i;
	dev_t dev_id = 0;			  		/* 设备号	*/

	printk(KERN_EMERG "%s()\n", __func__);

	printk(KERN_EMERG "numdev_major is %d!\r\n", numdev_major);
	printk(KERN_EMERG "numdev_minor is %d!\r\n", numdev_minor);
	
	/** 注册字符设备驱动 */
	/* 1、创建设备号 */
	/*	MAJOR(dev)， 就是对 dev 操作， 提取高 12 位主设备号 ；
		MINOR(dev) ， 就是对 dev 操作， 提取低 20 位数次设备号 ；
		MKDEV(ma,mi) ， 就是对主设备号和次设备号操作， 合并为 dev 类型。*/
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
	
	my_devices = kmalloc(DEVICE_NUM * sizeof(struct reg_dev), GFP_KERNEL);
	if(!my_devices){
		ret = -ENOMEM;
		goto fail;
	}
	memset(my_devices,0,DEVICE_NUM * sizeof(struct reg_dev));

	/* 4、创建设备类      */
	myclass = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(myclass)) return PTR_ERR(myclass);

	/* 设备注册到系统 */
	for(i=0;i<DEVICE_NUM;i++){
		my_devices[i].data = kmalloc(REGDEV_SIZE, GFP_KERNEL);
		if (my_devices[i].data) {
			memset(my_devices[i].data, 0, REGDEV_SIZE);		
			reg_init_cdev(&my_devices[i], i);  /*设备注册到系统*/
		}
		/* 5、创建设备 */
		my_devices[i].device = device_create(myclass, NULL, 
			MKDEV(numdev_major, numdev_minor + i), NULL, 
			"%s%d", DEVICE_NAME, i);
		if (IS_ERR(my_devices[i].device)) return PTR_ERR(my_devices[i].device);
	}

	return 0;

fail:
	/*注销设备号*/
	unregister_chrdev_region(dev_id, DEVICE_NUM);
	printk(KERN_EMERG "kmalloc is fail!\n");

	return ret;
}

/*驱动模块卸载函数*/
static void __exit regdriver_exit(void)
{
	int i=0;
	dev_t dev_id = MKDEV(numdev_major, numdev_minor); /* 设备号	*/

	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s()\n", __func__);

	/** 注销字符设备驱动 */
	for (i = 0; i < DEVICE_NUM; i++) {
		cdev_del(&(my_devices[i].cdev));		/*卸载字符设备*/	
		device_destroy(myclass, MKDEV(numdev_major, numdev_minor + i));/*摧毁设备*/
	}
	class_destroy(myclass);			/*摧毁类*/
	kfree(my_devices);				/** 释放内存 **/

	unregister_chrdev_region(dev_id, DEVICE_NUM); /* 注销设备号 */
}

/********************************************************************/

module_init(regdriver_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(regdriver_exit); /*卸载驱动时运行的函数， 如 rmmod*/

MODULE_LICENSE("Dual BSD/GPL"); /*Dual BSD/GPL 声明是开源的，没有内核版本限制*/
MODULE_AUTHOR("zcq");			/*声明作者*/
MODULE_DESCRIPTION("zcq reg");
