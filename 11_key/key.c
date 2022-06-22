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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/** 一般 原子操作 用于 变量 或者 位操作 **/
#include <asm/atomic.h>

/**
 互斥锁：是为上锁而优化；
 条件变量：是为等待而优化；
 信号量：既可上锁，也可等待，故开销大于前二者。
 **/

/**
 不同 进程间 通信 方式：
 	管道（血缘关系、匿名管道）；（使用简单）
 	FIFO（命名管道）；（使用简单）
 	本地套接字；（最稳定）
 	信号量；（要求原子操作，开销大于互斥锁/条件变量/读写锁）
 	信号；（要求原子操作，开销小，易丢失）
 	（系统/用户）信号（处理函数）；（开销最小，易丢失）
 	共享内存区/映射区；（无需系统调用、最快，但必须同步；如大量文件的复制粘贴）
 	消息队列；（任何时候读/写）
-----------------------------------
 不同 进程间 同步 方式：
	互斥锁；条件变量；（共享全局/静态变量，开销小）
 	文件/记录锁；
 	信号量；（要求原子操作，开销大于互斥锁/条件变量/读写锁）
-----------------------------------
 不同 线程间 同步 方式：
	互斥锁；条件变量；（共享全局/静态变量，开销小）
 	读写锁；（共享全局/静态变量，开销小）
 	信号量；（要求原子操作，开销大于互斥锁/条件变量/读写锁）
**/

#define KEY_CNT			1		/* 设备号个数 	*/
#define KEY_NAME		"key"	/* 名字 		*/

/* 定义按键值 */
#define KEY_0_VALUE		0XF0	/* 按键值 		*/
#define INVAKEY			0X00	/* 无效的按键值  */

/* 寄存器物理地址：设备树方式不需要 */

/* 映射后的寄存器虚拟地址指针：princtl 方式不需要 */

/* key设备结构体 */
struct key_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
	struct device_node	*nd; /* 设备节点 */
	int key_gpio;			// key 所使用的 Gled_switch PIO 编号

	/** 一般 原子操作 用于 变量 或者 位操作 **/
	/* 原子操作 API 函数 			描述
	ATOMIC_INIT(int i) 				定义原子变量的时候对其初始化。
	int atomic_read(atomic_t *v) 	读取 v 的值，并且返回。
	void atomic_inc(atomic_t *v) 	给 v 加 1，也就是自增。
	void atomic_dec(atomic_t *v) 	从 v 减 1，也就是自减
	void atomic_set(atomic_t *v, int i) 	向 v 写入 i 值。
	void atomic_add(int i, atomic_t *v) 	给 v 加上 i 值。
	void atomic_sub(int i, atomic_t *v) 	从 v 减去 i 值。
	int atomic_dec_return(atomic_t *v) 		从 v 减 1，并且返回 v 的值。
	int atomic_inc_return(atomic_t *v) 		给 v 加 1，并且返回 v 的值。
	int atomic_sub_and_test(int i, atomic_t *v) 从 v 减 i，如果结果为 0 就返回真，否则返回假
	int atomic_dec_and_test(atomic_t *v) 		从 v 减 1，如果结果为 0 就返回真，否则返回假
	int atomic_inc_and_test(atomic_t *v) 		给 v 加 1，如果结果为 0 就返回真，否则返回假
	int atomic_add_negative(int i, atomic_t *v) 给 v 加 i，如果结果为负就返回真，否则返回假
------------------------------------------------------------------------------------
	原子位操作 API 函数  					描述
	void set_bit(int nr, void *p) 		将 p 地址的第 nr 位置 1。
	void clear_bit(int nr,void *p) 		将 p 地址的第 nr 位清零。
	void change_bit(int nr, void *p) 	将 p 地址的第 nr 位进行翻转。
	int test_bit(int nr, void *p) 		获取 p 地址的第 nr 位的值。
	int test_and_set_bit(int nr, void *p) 	将 p 地址的第 nr 位置 1，并且返回 nr 位原来的值。
	int test_and_clear_bit(int nr, void *p) 将 p 地址的第 nr 位清零，并且返回 nr 位原来的值。
	int test_and_change_bit(int nr, void *p) 将 p 地址的第 nr 位翻转，并且返回 nr 位原来的值*/
	atomic_t keylock;		/* 按键值 		*/
};

struct key_dev keydev;		/* key设备 */

/*
 * @description	: 初始化按键IO，open函数打开驱动的时候
 * 				  初始化按键所使用的GPIO引脚。
 * @param 		: 无
 * @return 		: 无
 */
static int keyio_init(void)
{
	/* 获取设备树中的属性数据 设置所使用的GPIO */
	/* 1、获取设备节点 */
	keydev.nd = of_find_node_by_path("/key");
	if (keydev.nd== NULL) {
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到 key 所使用的编号 */
	keydev.key_gpio = of_get_named_gpio(keydev.nd ,"key-gpio", 0);
	if (keydev.key_gpio < 0) {
		printk("can't get key0\r\n");
		return -EINVAL;
	}
	printk("key_gpio num = %d\r\n", keydev.key_gpio);
	
	/* 初始化 key 所使用的 IO */
	gpio_request(keydev.key_gpio, "key0");	/* 请求IO */

	/* 3、设置为输入 */
	gpio_direction_input(keydev.key_gpio);
	return 0;
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int key_open(struct inode *inode, struct file *filp)
{
	int ret = keyio_init();				/* 初始化按键IO */
	if (ret < 0) {
		return ret;
	}

	filp->private_data = &keydev; 	/* 设置私有数据 */
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
static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int ret;
	int value;
	struct key_dev *dev = filp->private_data;	/* 获取私有数据 */

	if (gpio_get_value(dev->key_gpio) == 0) { 		/* key0按下 */
		while(!gpio_get_value(dev->key_gpio));		/* 等待按键释放 */
		atomic_set(&dev->keylock, KEY_0_VALUE);
	} else {	
		atomic_set(&dev->keylock, INVAKEY);		/* 无效的按键值 */
	}
	value = atomic_read(&dev->keylock);

	/* 	copy_to_user() 将数据从内核空间拷贝到用户空间,一般是在驱动中 chr_drv_read()用
参数1：内核驱动中的一个buffer
参数2：应用空间到一个buffer
参数3：个数   */
	ret = copy_to_user(buf, &value, sizeof(value));
	return ret;
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	/* copy_from_user(): 将数据从用户空间拷贝到内核空间,一般是在驱动中 chr_drv_write()用
参数1：应用驱动中的一个buffer
参数2：内核空间到一个buffer
参数3：个数
返回值：大于0，表示出错，剩下多少个没有拷贝成功; 等于0，表示正确  */

	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int key_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 设备操作函数 */
static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	/*  .write = key_write, // 此处无用 */
	.release = 	key_release,
};

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init mykey_init(void)
{
	/* 初始化原子变量 */
	atomic_set(&keydev.keylock, INVAKEY);


	/** 获取设备树中的属性数据 设置led所使用的GPIO
	1、获取设备节点
	2、获取设备树中的gpio属性，得到 key 所使用的编号
	3、设置为输入
	针对 按键，放在 open() 中。   	 **/


	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (keydev.major) {		/*  定义了设备号 */
		keydev.devid = MKDEV(keydev.major, 0);
		register_chrdev_region(keydev.devid, KEY_CNT, KEY_NAME);
	} else {						/* 没有定义设备号 */
		alloc_chrdev_region(&keydev.devid, 0, KEY_CNT, KEY_NAME);	/* 申请设备号 */
		keydev.major = MAJOR(keydev.devid);	/* 获取分配号的主设备号 */
		keydev.minor = MINOR(keydev.devid);	/* 获取分配号的次设备号 */
	}
	
	/* 2、初始化cdev */
//	keydev.cdev.owner = THIS_MODULE;
	cdev_init(&keydev.cdev, &key_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&keydev.cdev, keydev.devid, KEY_CNT);

	/* 4、创建类 */
	keydev.class = class_create(THIS_MODULE, KEY_NAME);
	if (IS_ERR(keydev.class)) {
		return PTR_ERR(keydev.class);
	}

	/* 5、创建设备 */
	keydev.device = device_create(keydev.class, NULL, keydev.devid, NULL, KEY_NAME);
	if (IS_ERR(keydev.device)) {
		return PTR_ERR(keydev.device);
	}
	
	return 0;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit mykey_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&keydev.cdev);/*  删除cdev */
	unregister_chrdev_region(keydev.devid, KEY_CNT); /* 注销设备号 */

	/* 注销掉类和设备 */
	device_destroy(keydev.class, keydev.devid);
	class_destroy(keydev.class);
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
