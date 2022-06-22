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

#include <linux/timer.h>
#include <linux/time.h>

#include <linux/spinlock.h>

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

#define TIMER_CNT        1        /* 设备号个数 	*/
#define TIMER_NAME        "timer"    /* 名字 		*/

#define CLOSE_CMD        (_IO(0XEF, 0x1))    /* 关闭定时器 */
#define OPEN_CMD        (_IO(0XEF, 0x2))    /* 打开定时器 */
#define SETPERIOD_CMD    (_IO(0XEF, 0x3))    /* 设置定时器周期命令 */

#define LEDON            1        /* 开灯 */
#define LEDOFF            0        /* 关灯 */

/* 寄存器物理地址：设备树方式不需要 */

/* 映射后的寄存器虚拟地址指针：princtl 方式不需要 */

/* timer设备结构体 */
struct timer_dev {
	dev_t devid;            /* 设备号 	 */
	struct cdev cdev;        /* cdev 	*/
	struct class *class;    /* 类 		*/
	struct device *device;    /* 设备 	 */
	int major;                /* 主设备号	  */
	int minor;                /* 次设备号   */
	struct device_node *nd; /* 设备节点 */
	int led_gpio;            // led 所使用的 Gled_switch PIO 编号

	/* 自旋锁 API 函数 					描述   spinlock_t
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
	void spin_lock_irq(spinlock_t *lock) 	禁止本地中断，并获取自旋锁。不推荐使用。
	void spin_unlock_irq(spinlock_t *lock) 	激活本地中断，并释放自旋锁。不推荐使用。
	void spin_lock_irqsave(spinlock_t *lock,
	 					unsigned long flags)保存中断状态，禁止本地中断，并获取自旋锁。
	void spin_unlock_irqrestore(spinlock_t *lock,
	 					unsigned long flags)将中断状态恢复到以前的状态，
	 					并且激活本地中断，释放自旋锁。
--------------------------------------------------------------------------------
	下半部(BH)也会竞争共享资源，在下半部里面使用自旋 API 函数：
	void spin_lock_bh(spinlock_t *lock) 	关闭下半部，并获取自旋锁。
	void spin_unlock_bh(spinlock_t *lock) 	打开下半部，并释放自旋锁。	 */
	spinlock_t lock;        /* 定义自旋锁 */

	/*	定时器 API 函数
比较函数
unkown 通常为 jiffies， known 通常是需要对比的值。可用于判断有没有超时。
time_after(unkown, known)		unkown > known	返回 真，否则 假
time_after_eq(unkown, known)	unkown >= known	返回 真，否则 假
time_before(unkown, known)		unkown < known	返回 真，否则 假
time_before_eq(unkown, known)	unkown <= known	返回 真，否则 假
--------------------------------------------------------------------------------
转换函数
将 jiffies 类型的参数 j 分别转换为对应的 毫秒、微秒、纳秒：
int jiffies_to_msecs(const unsigned long j)
int jiffies_to_usecs(const unsigned long j)
u64 jiffies_to_nsecs(const unsigned long j)
将 毫秒、微秒、纳秒 转换为 jiffies 类型：
long msecs_to_jiffies(const unsigned int m)
long usecs_to_jiffies(const unsigned int u)
unsigned long nsecs_to_jiffies(u64 n)
--------------------------------------------------------------------------------
void init_timer(struct timer_list *timer)	初始化定时器
void add_timer(struct timer_list *timer)	向 Linux 内核注册定时器，定时器就会开始运行
int del_timer(struct timer_list * timer)	删除定时器，无论定时器是否激活
		在多处理器系统上，定时器可能会在其他的处理器上运行，
	因此在调用 del_timer 函数删除定时器之前要先等待其他处理器的定时处理器函数退出。
	返回值： 0，定时器还没被激活； 1，定时器已经激活
int del_timer_sync(struct timer_list *timer)	是 del_timer 函数的同步版，
	等待其他处理器使用完定时器再删除，del_timer_sync 不能使用在中断上下文中。
int mod_timer(struct timer_list *timer, unsigned long expires)	修改定时值
	 如果定时器还没有激活的话， mod_timer 函数会激活定时器！
	返回值： 0，调用 mod_timer 函数前定时器未被激活； 1，调用 mod_timer 函数前定时器已被激活。
--------------------------------------------------------------------------------
有时候需要在内核中实现短延时，尤其是在 Linux 驱动中：
void ndelay(unsigned long nsecs)	纳秒
void udelay(unsigned long usecs) 	微秒
void mdelay(unsigned long mseces)	毫秒		 */
	int timeperiod;        /* 定时周期,单位为ms */
	struct timer_list timer;/* 定义一个定时器 */
};

struct timer_dev timerdev;    /* timer设备 */

/*
 * @description	: 初始化LED灯IO，open函数打开驱动的时候
 * 				  初始化LED灯所使用的GPIO引脚。
 * @param 		: 无
 * @return 		: 无
 */
static int led_init(void) {
	int ret = 0;

	/* 获取设备树中的属性数据 设置led所使用的GPIO */
	/* 1、获取设备节点 */
	timerdev.nd = of_find_node_by_path("/gpioled");
	if (timerdev.nd == NULL) {
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到 led 所使用的编号 */
	timerdev.led_gpio = of_get_named_gpio(timerdev.nd, "led-gpio", 0);
	if (timerdev.led_gpio < 0) {
		printk("can't get led\r\n");
		return -EINVAL;
	}
	printk("led-gpio num = %d\r\n", timerdev.led_gpio);

	/* 初始化 led 所使用的 IO */
	gpio_request(timerdev.led_gpio, "led");        /* 请求IO 	*/

	/* 3、设置为输出 */
	ret = gpio_direction_output(timerdev.led_gpio, 1);
	if (ret < 0) {
		printk("can't set gpio!\r\n");
	}
	return 0;
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int timer_open(struct inode *inode, struct file *filp)
{
	int ret = led_init();      /* 初始化LED IO */
	if (ret < 0) {
		return ret;
	}

	timerdev.timeperiod = 1000;        /* 默认周期为1s */
	filp->private_data = &timerdev;    /* 设置私有数据 */
	return 0;
}

/*
 * @description		: ioctl 函数，
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - cmd 	: 应用程序发送过来的命令
 * @param - arg 	: 参数
 * @return 			: 0 成功;其他 失败
 */
static long timer_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct timer_dev *dev = (struct timer_dev *) filp->private_data;	/* 获取私有数据 */
	int timerperiod;
	unsigned long flags;

	switch (cmd) {
		case CLOSE_CMD:        /* 关闭定时器 */
			del_timer_sync(&dev->timer);
			break;
		case OPEN_CMD:        /* 打开定时器 */
		spin_lock_irqsave(&dev->lock, flags); //保存中断状态，禁止本地中断，并获取自旋锁。
			timerperiod = dev->timeperiod;
			spin_unlock_irqrestore(&dev->lock, flags);//将中断状态恢复到以前的状态，并且激活本地中断，释放自旋锁。
			mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
			break;
		case SETPERIOD_CMD: /* 设置定时器周期 */
		spin_lock_irqsave(&dev->lock, flags); //保存中断状态，禁止本地中断，并获取自旋锁。
			dev->timeperiod = (int)arg;
			spin_unlock_irqrestore(&dev->lock, flags);//将中断状态恢复到以前的状态，并且激活本地中断，释放自旋锁。
			mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
			break;
		default:
			break;
	}
	return 0;
}

/* 设备操作函数 */
static struct file_operations timer_fops = {
		.owner = THIS_MODULE,
		.open = timer_open,
		.unlocked_ioctl = timer_unlocked_ioctl,
};

/* 定时器服务函数/定时器回调函数 */
// struct timeval oldtv;
void timer_callback(unsigned long arg) {
	struct timer_dev *dev = (struct timer_dev *) arg;
	static int sta = 1;
	int timerperiod;
	unsigned long flags;

	// struct timeval tv;
	// do_gettimeofday(&tv);
    // printk("%s: %ld, %ld\n", __func__,
    //     tv.tv_sec - oldtv.tv_sec,        //与上次中断间隔 s
    //     tv.tv_usec- oldtv.tv_usec); 
	// oldtv = tv;
    // tm.expires = jiffies+1*HZ;    
    // add_timer(&tm);        //重新开始计时

	sta = !sta;        /* 每次都取反，实现LED灯反转 */
	gpio_set_value(dev->led_gpio, sta);

	/* 重启定时器 */
	spin_lock_irqsave(&dev->lock, flags); //保存中断状态，禁止本地中断，并获取自旋锁。
	timerperiod = dev->timeperiod;
	spin_unlock_irqrestore(&dev->lock, flags);//将中断状态恢复到以前的状态，并且激活本地中断，释放自旋锁。

	/* mod_timer 函数用于修改定时值，如果定时器还没有激活的话， mod_timer 函数会激活定时器 */
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
}

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init timer_init(void)
{
	/* 初始化自旋锁 */
	spin_lock_init(&timerdev.lock);


	/** 获取设备树中的属性数据 设置led所使用的GPIO
	1、获取设备节点
	2、获取设备树中的gpio属性，得到 key 所使用的编号
	3、设置为输入
	针对 定时器，放在 open() 中。   	 **/


	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (timerdev.major) {        /*  定义了设备号 */
		timerdev.devid = MKDEV(timerdev.major, 0);
		register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
	} else {                        /* 没有定义设备号 */
		alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);    /* 申请设备号 */
		timerdev.major = MAJOR(timerdev.devid);    /* 获取分配号的主设备号 */
		timerdev.minor = MINOR(timerdev.devid);    /* 获取分配号的次设备号 */
	}

	/* 2、初始化cdev */
	timerdev.cdev.owner = THIS_MODULE;
	cdev_init(&timerdev.cdev, &timer_fops);

	/* 3、添加一个cdev */
	cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);

	/* 4、创建类 */
	timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
	if (IS_ERR(timerdev.class)) {
		return PTR_ERR(timerdev.class);
	}

	/* 5、创建设备 */
	timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
	if (IS_ERR(timerdev.device)) {
		return PTR_ERR(timerdev.device);
	}

	/** 6、初始化 timer，设置定时器处理函数,还未设置周期，所有不会激活定时器 */
	init_timer(&timerdev.timer); // 初始化内核定时器
	timerdev.timer.function = timer_callback; // 定时器回调函数
	timerdev.timer.data = (unsigned long) &timerdev; // 要传递给 function 函数的参数
	
	// timerdev.timeperiod = 1000;        /* 默认周期为1s */
	// timerdev.timer.expires = jiffies + timerdev.timeperiod*HZ; //定时时间
	// add_timer(&tm); //向 Linux 内核注册定时器，定时器就会开始运行
	// do_gettimeofday(&oldtv);        //获取当前时间

	return 0;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit timer_exit(void) {

	/** 7、删除 timer*/
	del_timer_sync(&timerdev.timer);        /* 删除 timer */
#if 0
del_timer(&timerdev.tiemr);
#endif
	gpio_set_value(timerdev.led_gpio, 1);    /* 卸载驱动的时候关闭LED */

	/* 注销字符设备驱动 */
	cdev_del(&timerdev.cdev);/*  删除cdev */
	unregister_chrdev_region(timerdev.devid, TIMER_CNT); /* 注销设备号 */

	device_destroy(timerdev.class, timerdev.devid);
	class_destroy(timerdev.class);
}

module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
