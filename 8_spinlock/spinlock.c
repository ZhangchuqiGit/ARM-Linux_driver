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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/spinlock.h>

/** 原子操作 只能对 整形变量 或者 位进行保护，
 但在实际的使用环境中怎么可能只有整形变量或位这么简单的 临界区。
 如 结构体变量就不是整型变量，在线程 A 对结构体变量使用期间，应该禁止其他的线程来访问此结构体变量，
 这些工作是原子操作都不能胜任，需要自旋锁。
-------
 	在编写驱动程序的时候我们必须考虑到驱动的可移植性，
 因此不管你用的是单核的还是多核的 SOC，都将其当做多核 SOC 来编写驱动程序。
-------
 	此 自旋锁 API 函数 适用于 SMP 或 支持抢占的 单CPU下 线程之间的并发访问，
也就是用于线程与线程之间，被自旋锁保护的临界区一定不能调用任何能够引起睡眠和阻塞的 API 函数，
否则的话会可能会导致死锁现象的发生。
自旋锁会自动禁止抢占，也就说当线程 A 得到锁以后会暂时禁止内核抢占。
如果线程 A 在持有锁期间进入了休眠状态，那么线程 A 会自动放弃 CPU 使用权。
线程 B 开始运行，线程 B 也想要获取锁，但是此时锁被 A 线程持有，而且内核抢占还被禁止了！
线程 B 无法被调度出去，那么线程 A 就无法运行，锁也就无法释放，死锁发生了！
-------
 “自旋”也就是“原地打转”的意思，那就等待自旋锁的线程会一直处于自旋状态，
 这样会浪费处理器时间，降低系统性能，所以自旋锁的持有时间不能太长。
 自旋锁适用于短时期的轻量级加锁，如果遇到需要长时间持有锁的场景那就需要换其他的方法了。
 自旋锁保护的临界区内不能调用任何可能导致线程休眠的 API 函数，否则的话可能导致死锁。
 不能递归申请自旋锁，因为一旦通过递归的方式申请一个你正在持有的锁，
 那么你就必须“自旋”，等待锁被释放，然而你正处于“自旋”状态，根本没法释放锁，把自己锁死了！ **/

/* 在自旋锁的基础上还衍生出了其他特定场合使用的锁，
 这些锁在驱动中其实用的不多，更多的是在 Linux 内核中使用。
--------------------------------------------------------------------------------
读写锁（读写自旋锁）rwlock_t
	一次只能允许一个写操作，也就是只能一个线程持有写锁，而且不能进行读操作。
	没有写操作的时候允许一个或多个线程持有读锁，可以进行并发的读操作。
-------------------------------------
读写自旋锁函数 							描述
	DEFINE_RWLOCK(rwlock_t lock) 		定义并初始化读写锁
	void rwlock_init(rwlock_t *lock) 	初始化读写锁。
------------------------------------- 读锁
	void read_lock(rwlock_t *lock) 		获取读锁。
	void read_unlock(rwlock_t *lock) 	释放读锁。
----
中断里面可以使用自旋锁，在获取锁之前一定要先禁止本地中断(也就是本 CPU 中断，
对于多核 SOC 来说会有多个 CPU 核)，否则可能导致锁死现象的发生：
	void read_lock_irq(rwlock_t *lock) 		禁止本地中断，并且获取读锁。不推荐使用。
	void read_unlock_irq(rwlock_t *lock) 	打开本地中断，并且释放读锁。不推荐使用。
	void read_lock_irqsave(rwlock_t *lock,
				unsigned long flags)	保	存中断状态，禁止本地中断，并获取读锁。
	void read_unlock_irqrestore(rwlock_t *lock,
				unsigned long flags)	将中断状态恢复到以前的状态，并且激活本地中断，释放读锁。
----
下半部(BH)也会竞争共享资源，在下半部里面使用自旋 API 函数：
	void read_lock_bh(rwlock_t *lock) 	关闭下半部，并获取读锁。
	void read_unlock_bh(rwlock_t *lock) 打开下半部，并释放读锁。
------------------------------------- 写锁
	void write_lock(rwlock_t *lock) 		获取写锁。
	void write_unlock(rwlock_t *lock) 		释放写锁。
----
中断里面可以使用自旋锁，在获取锁之前一定要先禁止本地中断(也就是本 CPU 中断，
对于多核 SOC 来说会有多个 CPU 核)，否则可能导致锁死现象的发生：
	void write_lock_irq(rwlock_t *lock) 	禁止本地中断，并且获取写锁。不推荐使用。
	void write_unlock_irq(rwlock_t *lock) 	打开本地中断，并且释放写锁。不推荐使用。
	void write_lock_irqsave(rwlock_t *lock,
					unsigned long flags) 	保存中断状态，禁止本地中断，并获取写锁。
	void write_unlock_irqrestore(rwlock_t *lock,
					unsigned long flags)	将中断状态恢复到以前的状态，并且激活本地中断，释放读锁。
----
下半部(BH)也会竞争共享资源，在下半部里面使用自旋 API 函数：
	void write_lock_bh(rwlock_t *lock) 关闭下半部，并获取读锁。
	void write_unlock_bh(rwlock_t *lock) 打开下半部，并释放读锁。
--------------------------------------------------------------------------------
顺序锁（顺序读写自旋锁）seqlock_t
 	顺序锁在读写锁的基础上衍生而来的，使用读写锁的时候读操作和写操作不能同时进行。
使用顺序锁的话可以允许在写的时候进行读操作，也就是实现同时读写，但是不允许同时进行并发的写操作。
如果在读的过程中发生了写操作，最好重新进行读取，保证数据完整性。
 	顺序锁保护的资源不能是指针，因为如果在写操作的时候可能会导致指针无效，
而这个时候恰巧有读操作访问指针的话就可能导致意外发生，比如读取野指针导致系统崩溃。
-------------------------------------
顺序锁 									描述
	DEFINE_SEQLOCK(seqlock_t sl) 		定义并初始化顺序锁
	void seqlock_ini seqlock_t *sl) 	初始化顺序锁。
------------------------------------- 写操作
	void write_seqlock(seqlock_t *sl) 	获取写顺序锁。
	void write_sequnlock(seqlock_t *sl) 释放写顺序锁。
----
中断里面可以使用自旋锁，在获取锁之前一定要先禁止本地中断(也就是本 CPU 中断，
对于多核 SOC 来说会有多个 CPU 核)，否则可能导致锁死现象的发生：
	void write_seqlock_irq(seqlock_t *sl) 	禁止本地中断，并且获取写顺序锁。不推荐使用。
	void write_sequnlock_irq(seqlock_t *sl) 打开本地中断，并且释放写顺序锁。不推荐使用。
	void write_seqlock_irqsave(seqlock_t *sl,
				unsigned long flags)	保存中断状态，禁止本地中断，并获取写顺序锁。
	void write_sequnlock_irqrestore(seqlock_t *sl,
				unsigned long flags) 	将中断状态恢复到以前的状态，并且激活本地中断，释放写顺序锁。
----
下半部(BH)也会竞争共享资源，在下半部里面使用自旋 API 函数：
	void write_seqlock_bh(seqlock_t *sl) 	关闭下半部，并获取写读锁。
	void write_sequnlock_bh(seqlock_t *sl) 	打开下半部，并释放写读锁。
------------------------------------- 读操作
	unsigned read_seqbegin(const seqlock_t *sl)	读单元访问共享资源的时候调用此函数，
 							此函数会返回顺序锁的顺序号。
	unsigned read_seqretry(const seqlock_t *sl, unsigned start)
 							读结束以后调用此函数检查在读的过程中有没有对资源进行写操作，
 							如果有的话就要重读。   */

#define GPIOLED_CNT            1            /* 设备号个数 */
#define GPIOLED_NAME        "gpioled"    /* 名字 */
#define LEDOFF                0            /* 关灯 */
#define LEDON                1            /* 开灯 */

/* gpioled设备结构体 */
struct gpioled_dev {
	dev_t devid;                /* 设备号 	 */
	struct cdev cdev;            /* cdev 	*/
	struct class *class;        /* 类 		*/
	struct device *device;        /* 设备 	 */
	int major;                    /* 主设备号	  */
	int minor;                    /* 次设备号   */
	struct device_node *nd;    /* 设备节点 */
	int led_gpio;            // led 所使用的 Gled_switch PIO 编号

	/* 自旋锁 API 函数 					描述
	DEFINE_SPINLOCK(spinlock_t lock) 	定义并初始化一个自选变量。
	int spin_lock_init(spinlock_t *lock) 初始化自旋锁。
	void spin_lock(spinlock_t *lock) 	获取指定的自旋锁，也叫做加锁。
	void spin_unlock(spinlock_t *lock) 	释放指定的自旋锁。
	int spin_trylock(spinlock_t *lock) 	尝试获取指定的自旋锁，如果没有获取到就返回 0
	int spin_is_locked(spinlock_t *lock) 检查指定的自旋锁是否被获取，如果没有被获取就返回非 0，否则返回 0。
	此 自旋锁 API 函数 适用于 SMP 或 支持抢占的 单CPU下 线程之间的并发访问，
也就是用于线程与线程之间，被自旋锁保护的临界区一定不能调用任何能够引起睡眠和阻塞的 API 函数，
否则的话会可能会导致死锁现象的发生。
自旋锁会自动禁止抢占，也就说当线程 A 得到锁以后会暂时禁止内核抢占。
如果线程 A 在持有锁期间进入了休眠状态，那么线程 A 会自动放弃 CPU 使用权。
线程 B 开始运行，线程 B 也想要获取锁，但是此时锁被 A 线程持有，而且内核抢占还被禁止了！
线程 B 无法被调度出去，那么线程 A 就无法运行，锁也就无法释放，死锁发生了！
--------------------------------------------------------------------------------
	 中断里面可以使用自旋锁，在获取锁之前一定要先禁止本地中断(也就是本 CPU 中断，
对于多核 SOC 来说会有多个 CPU 核)，否则可能导致锁死现象的发生。
	中断处理 自旋锁 API 函数 					描述
	void spin_lock_irq(spinlock_t *lock) 	禁止本地中断，并获取自旋锁。
	void spin_unlock_irq(spinlock_t *lock) 	激活本地中断，并释放自旋锁。
	void spin_lock_irqsave(spinlock_t *lock,
	 					unsigned long flags)保存中断状态，禁止本地中断，并获取自旋锁。
	void spin_unlock_irqrestore(spinlock_t *lock,
	 					unsigned long flags)将中断状态恢复到以前的状态，
	 					并且激活本地中断，释放自旋锁。
--------------------------------------------------------------------------------
	下半部(BH)也会竞争共享资源，在下半部里面使用自旋 API 函数：
	void spin_lock_bh(spinlock_t *lock) 	关闭下半部，并获取自旋锁。
	void spin_unlock_bh(spinlock_t *lock) 	打开下半部，并释放自旋锁。	 */
	int dev_stats;      /* 设备使用状态，0，设备未使用;  >0,设备已经被使用 */
	spinlock_t lock;    /* 自旋锁 */
};

struct gpioled_dev gpioled;    /* led设备 */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp) {
	unsigned long flags;
	spin_lock_irqsave(&gpioled.lock, flags); /* 上锁 */
	if (gpioled.dev_stats) {                    /* 如果设备被使用了 */
		spin_unlock_irqrestore(&gpioled.lock, flags);/* 解锁 */
		return -EBUSY;
	}
	gpioled.dev_stats++;    /* 如果设备没有打开，那么就标记已经打开了 */
	spin_unlock_irqrestore(&gpioled.lock, flags);/* 解锁 */

	filp->private_data = &gpioled; /* 设置私有数据 */
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
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt) {
	/* 	copy_to_user() 将数据从内核空间拷贝到用户空间,一般是在驱动中 chr_drv_read()用
参数1：内核驱动中的一个buffer
参数2：应用空间到一个buffer
参数3：个数   */

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
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt) {
	int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;
	struct gpioled_dev *dev = filp->private_data;

	/* copy_from_user(): 将数据从用户空间拷贝到内核空间,一般是在驱动中 chr_drv_write()用
参数1：应用驱动中的一个buffer
参数2：内核空间到一个buffer
参数3：个数
返回值：大于0，表示出错，剩下多少个没有拷贝成功; 等于0，表示正确  */
	retvalue = copy_from_user(databuf, buf, cnt);
	if (retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];        /* 获取状态值 */

	if (ledstat == LEDON) {
		gpio_set_value(dev->led_gpio, 0);    /* 打开LED灯 */
	} else if (ledstat == LEDOFF) {
		gpio_set_value(dev->led_gpio, 1);    /* 关闭LED灯 */
	}
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp) {
	unsigned long flags;
	struct gpioled_dev *dev = filp->private_data;

	/* 关闭驱动文件的时候将dev_stats减1 */
	spin_lock_irqsave(&dev->lock, flags);    /* 上锁 */
	if (dev->dev_stats) {
		dev->dev_stats--;
	}
	spin_unlock_irqrestore(&dev->lock, flags);/* 解锁 */

	return 0;
}

/* 设备操作函数 */
static struct file_operations gpioled_fops = {
		.owner = THIS_MODULE,
		.open = led_open,
/*        .read = led_read, // 此处无用 */
		.write = led_write,
		.release =    led_release,
};

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init led_init(void) {
	int ret = 0;

	/*  初始化自旋锁 */
	spin_lock_init(&gpioled.lock);

	/* 获取设备树中的属性数据 设置led所使用的GPIO */
	/* 1、获取设备节点：gpioled */
	gpioled.nd = of_find_node_by_path("/gpioled");
	if (gpioled.nd == NULL) {
		printk("gpioled node not find!\r\n");
		return -EINVAL;
	} else {
		printk("gpioled node find!\r\n");
	}

	/* 2、 获取设备树中的gpio属性，得到LED所使用的LED编号 */
	gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpio", 0);
	if (gpioled.led_gpio < 0) {
		printk("can't get led-gpio");
		return -EINVAL;
	}
	printk("led-gpio num = %d\r\n", gpioled.led_gpio);

	/* 3、设置GPIO1_IO03为输出，并且输出高电平，默认关闭LED灯 */
	ret = gpio_direction_output(gpioled.led_gpio, 1);
	if (ret < 0) {
		printk("can't set gpio!\r\n");
	}



	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (gpioled.major) {        /*  定义了设备号 */
		gpioled.devid = MKDEV(gpioled.major, 0);
		register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
	} else {                        /* 没有定义设备号 */
		alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);    /* 申请设备号 */
		gpioled.major = MAJOR(gpioled.devid);    /* 获取分配号的主设备号 */
		gpioled.minor = MINOR(gpioled.devid);    /* 获取分配号的次设备号 */
	}
	printk("gpioled major=%d,minor=%d\r\n", gpioled.major, gpioled.minor);

	/* 2、初始化cdev */
	gpioled.cdev.owner = THIS_MODULE;
	cdev_init(&gpioled.cdev, &gpioled_fops);

	/* 3、添加一个cdev */
	cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);

	/* 4、创建类 */
	gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
	if (IS_ERR(gpioled.class)) {
		return PTR_ERR(gpioled.class);
	}

	/* 5、创建设备 */
	gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
	if (IS_ERR(gpioled.device)) {
		return PTR_ERR(gpioled.device);
	}

	return 0;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit led_exit(void) {
	/* 注销字符设备驱动 */
	cdev_del(&gpioled.cdev);/*  删除cdev */
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT); /* 注销设备号 */

	device_destroy(gpioled.class, gpioled.devid);
	class_destroy(gpioled.class);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
