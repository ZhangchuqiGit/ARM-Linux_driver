
/** 信号量（semaphore）是一种提供不同进程之间或者一个给定进程不同线程之间的同步。
 	POSIX、SystemV 信号随内核持续。
 	Linux操作系统中，POSIX 有名 信号量 创建在虚拟文件系统中，一般挂载在 /dev/shm，
 其名字以 sem.somename 的形式存在。
 	信号量初始化的值的大小一般用于表示可用资源的数（例如缓冲区大小，之后代码中体现）；
 如果初始化为 1，则称之二值信号量，二值信号量的功能就有点像互斥锁了。
 不同的是：互斥锁的加锁和解锁必须在同一线程执行，而信号量的挂出却不必由执行等待操作的线程执行。
--------------------------------------------------------------------------
 有名信号量（基于路径名 /dev/sem/sem.zcq）：通常用于 不同进程之间的同步
 无名信号量（基于内存的信号量）：通常用于 一个给定进程的 不同线程之间的同步
--------------------------------------------------------------------------
 注意：fork()的子进程，通常不共享父进程的内存空间，
 子进程是在父进程的副本上启动的，它跟共享内存区不是一回事。
 不同进程之间的同步 若使用 无名信号量（基于内存的信号量），要考虑指针或地址的关联。
--------------------------------------------------------------------------
 一个信号量的最大值  SEM_VALUE_MAX  (2147483647)  **/

/**	信号量 广泛用于进程或线程间的同步和互斥，信号量本质上是一个非负的整数计数器，
它被用来控制对公共资源的访问。可根据操作信号量值的结果判断是否对公共资源具有访问的权限，
当信号量值大于 0 时，则可以访问，否则将阻塞。PV 原语是对信号量的操作，
一次 P (wait) 操作使信号量减１，一次 V (post) 操作使信号量加１。 **/

/* 函数 							描述
DEFINE_SEAMPHORE(name) 				定义一个信号量，并且设置信号量的值为 1。
void sema_init(struct semaphore *sem, int val) 	初始化信号量 sem，设置信号量值为 val。
void down(struct semaphore *sem)	获取信号量，
 			因为会导致休眠且不能被信号打断，因此不能在中断中使用。
int down_trylock(struct semaphore *sem);	尝试获取信号量，
 			如果能获取到信号量就获取，并且返回 0。如果不能就返回非 0，并且不会进入休眠。
int down_interruptible(struct semaphore *sem)	获取信号量，和 down 类似，
 			而使用此函数进入休眠以后是可以被信号打断的。
void up(struct semaphore *sem) 释放信号量	  */

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

//原子操作的函数头文件
#include <asm/atomic.h>
#include <asm/types.h>

#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/backlight.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <linux/input.h>

/********************************************************************/

#define DRIVER_NAME 		"zcq_misc_pwm_beep"

#define PWM_IOCTL_SET_FREQ 1
#define PWM_IOCTL_STOP 0

#define NS_IN_1HZ (1000000000UL)

//蜂鸣器 PWM_ID  0
#define BUZZER_PWM_ID 0

//定义一个结构体指针
struct pwm_device *pwm_buzzer;
//定义一个结构体信号量指针,因为信号量与锁的机制差不多
//Mutex是一把钥匙，一个人拿了就可进入一个房间，出来的时候把钥匙交给队列的第一个。一般的用法是用于串行化对critical section代码的访问，保证这段代码不会被并行的运行。
//Semaphore是一件可以容纳N人的房间，如果人不满就可以进去，如果人满了，就要等待有人出来。对于N=1的情况，称为binary semaphore。一般的用法是，用于限//制对于某一资源的同时访问。
struct semaphore lock;

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

//设置频率:HZ
// static void pwm_set_freq(unsigned long freq)
static void pwm_set_freq(int rate /* 0% ~ 100% */ ,
						unsigned long freq )
{
	int duty_ns = 0; 	/*pwm占空比时间，单位为ns*/
	int period_ns = 0; 	/*pwm周期时间，单位为ns*/ 

	if (rate < 0 || rate > 100) rate = 50;
	if (freq == 0) freq = 100;

	period_ns = NS_IN_1HZ / freq; 	/*pwm周期时间，单位为ns*/ 
	// duty_ns = period_ns / 2; 	/*pwm占空比 50% */
	duty_ns = rate * period_ns / 100; 		/*pwm占空比时间，单位为ns*/

	pwm_config(pwm_buzzer, duty_ns, period_ns);	/* 配置 PWM */
	pwm_enable(pwm_buzzer);
}

static void pwm_stop(void)
{
	// pwm_config(pwm_buzzer, 0, NS_IN_1HZ / 100);
	pwm_disable(pwm_buzzer);
}

/********************************************************************/

//open方法函数，来源于operations结构体，主要打开pwm的操作
static int iTop4412_pwm_open(struct inode *inode, struct file *filp)
{
	if (!down_trylock(&lock)) //尝试加锁，如果失败返回0
		return 0;
	else
		return -EBUSY;
}

static int iTop4412_pwm_close(struct inode *inode, struct file *filp)
{
	up(&lock); /*解锁, 释放信号量*/
	return 0;
}

//控制io口方法函数，来源于 operations 结构体,其实就是上层系统调用传入一条命令，
//驱动识别命令，然后执行相应过程。
static long iTop4412_pwm_ioctl(struct file *filp, unsigned int cmd,
							   unsigned long arg)
{
	switch (cmd) {
		case PWM_IOCTL_SET_FREQ:
			pwm_set_freq(50, arg); //设置频率:HZ
			break;
		case PWM_IOCTL_STOP:
		default:
			pwm_stop();
			break;
	}
	return 0;
}

/********************************************************************/

/* 设备操作结构体 */
static struct file_operations iTop4412_pwm_ops = {
	.owner = THIS_MODULE, /*表示本模块拥有*/
	.open = iTop4412_pwm_open,
	// .read = miscbeep_read, // 此处无用
	.release = iTop4412_pwm_close,
	.unlocked_ioctl = iTop4412_pwm_ioctl,
};

/* MISC 设备结构体 */
static struct miscdevice iTop4412_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR, // 动态子设备号
	.name = DRIVER_NAME,		 // 设备名字
	.fops = &iTop4412_pwm_ops,
};

/** *************** platform 驱动 结构体 函数 ********************* **/

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int pwmbeep_probe(struct platform_device *pdev)
{
	int ret = 0;

	/* 获取设备树中的属性数据 设置所使用的GPIO */
	/* 1、获取设备节点 */
#if 0
	/** 若驱动程序采用 设备树 + platform 方式, 接口： /sys/devices/platform/zcq_beep */
	miscbeep.node = of_find_node_by_path("/zcq_beep"); /*设备节点*/
#else
	miscbeep.node = pdev->dev.of_node; /*设备节点*/
#endif
	if (miscbeep.node == NULL) {
		printk("beep node not find!\r\n");
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到所使用的编号 */
	miscbeep.beep_gpio = of_get_named_gpio(miscbeep.node, "gpios", 0);
	if (miscbeep.beep_gpio == -EPROBE_DEFER) return miscbeep.beep_gpio;
	if (miscbeep.beep_gpio < 0) {
		dev_err(&pdev->dev, "error acquiring zcq_beep gpio: %d\n", 
			miscbeep.beep_gpio);
		return -EINVAL;
	}

	gpio_free(miscbeep.beep_gpio); /*释放已经被占用的 IO*/

	/* 3、申请 gpio */
	ret = devm_gpio_request_one(&pdev->dev, miscbeep.beep_gpio, 0, DRIVER_NAME);
	if(ret) {
			dev_err(&pdev->dev, "error requesting zcq_beep gpio: %d\n", ret);
			return ret;
	}

	/* 4、设置为输出 */
	ret = gpio_direction_output(miscbeep.beep_gpio, 0);
	if (ret < 0) {
		printk("can't set gpio output!\r\n");
	}

	/*申请一个 PWM 资源*/
	pwm_buzzer = pwm_request(BUZZER_PWM_ID, DRIVER_NAME);
	if (IS_ERR(pwm_buzzer)) {
		printk("request pwm %d for %s failed\n", BUZZER_PWM_ID, DRIVER_NAME);
		return -ENODEV;
	}

	pwm_stop();

	sema_init(&lock, 1); /* 初始化信号量 sem，设置信号量值为 val。*/

	/** 一般情况下会注册对应的字符设备 cdev，但是这里我们使用 MISC 设备，
	 * 所以我们不需要自己注册字符设备驱动，只需要注册misc设备驱动即可	 */
	ret = misc_register(&iTop4412_misc_dev); /*注册 misc 杂项设备*/

	return ret;
}

/*移除驱动*/
static int pwmbeep_remove(struct platform_device *dev)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s()\n", __func__);

	pwm_stop();
	gpio_set_value(miscbeep.beep_gpio, 0); /* 卸载驱动的时候关闭beer */

	misc_deregister(&iTop4412_misc_dev); 	/*注销 misc 杂项设备*/
	gpio_free(miscbeep.beep_gpio);			 /*释放已经被占用的 IO*/
	return 0;
}

/** *************** 设备树 *.dts 匹配列表 ********************* **/

static const struct of_device_id beep_of_match[] = {
	{.compatible = "zcq,misc,pwm"}, /* 兼容属性 */
	{},
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE(of, beep_of_match);

/** *************** platform 驱动 结构体 ********************* **/

static struct platform_driver beep_driver = {
	.probe = pwmbeep_probe,   /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.remove = pwmbeep_remove, /*移除驱动*/
	// .shutdown = hello_shutdown,			/*关闭驱动*/
	// .suspend = hello_suspend,			/*悬挂（休眠）驱动*/
	// .resume = hello_resume,				/*驱动恢复后要做什么*/
	.driver = {
		.name = DRIVER_NAME, 	/* 名字，用于驱动和设备的匹配 */
		.owner = THIS_MODULE,	/*表示本模块拥有*/
		.of_match_table = of_match_ptr(beep_of_match), /* 设备树匹配表 */
	}
};

/********************************************************************/
#if 0
/*驱动模块自动加载、卸载函数*/
module_platform_driver(beep_driver);
#else
/*驱动模块加载函数*/
static int __init pwmbeep_init(void)
{
	printk(KERN_INFO "%s()\n", __func__);
	return platform_driver_register(&beep_driver); /*向系统注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit pwmbeep_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_INFO "%s()\n", __func__);
	platform_driver_unregister(&beep_driver); /*向系统注销驱动*/
}

module_init(pwmbeep_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(pwmbeep_exit); /*卸载驱动时运行的函数， 如 rmmod*/
#endif
/********************************************************************/

MODULE_LICENSE("GPL"); 	/* 声明开源，没有内核版本限制 */
MODULE_AUTHOR("zcq");   /* 声明作者 */
MODULE_DESCRIPTION("zcq pwmbeep");
