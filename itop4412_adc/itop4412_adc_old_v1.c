
/** itop4412 一共有 4 路 ADC 接口
 * 网络标号是 XadcAIN0~XadcAIN3 
 * 开发板自带的 ADC 电路， ADC 接的是滑动变阻器 */

/*
 互斥锁：是为上锁而优化；
 条件变量：是为等待而优化；
 信号量：既可上锁，也可等待，故开销大于前二者。
 **/

/*
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

/* 互斥体的 API 函数 			描述
DEFINE_MUTEX(name) 				定义并初始化一个 mutex 变量。
void mutex_init(mutex *lock) 	初始化 mutex。
void mutex_lock(struct mutex *lock) 	获取 mutex，也就是给 mutex 上锁。如果获取不到就进休眠。
void mutex_unlock(struct mutex *lock) 	释放 mutex，也就给 mutex 解锁。
int mutex_trylock(struct mutex *lock) 	尝试获取 mutex，如果成功就返回 1，如果失败就返回 0。
int mutex_is_locked(struct mutex *lock) 判断 mutex 是否被获取，如果是的话就返回1，否则返回 0。
int mutex_lock_interruptible(struct mutex *lock) 使用此函数获取信号量失败进入休眠且可以被信号打断。
 */

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

#include <plat/adc.h>

/********************************************************************/

#define DEVICE_NAME "adc"
#define DRIVER_NAME "adc_ctl"

typedef struct
{
	struct mutex lock;
	struct s3c_adc_client *client;
	int channel;
} ADC_DEV;
static ADC_DEV adcdev;

/********************************************************************/

static inline int exynos_adc_read_ch(void) {
	int ret = mutex_lock_interruptible(&adcdev.lock);
	if (ret < 0) return ret;
	ret = s3c_adc_read(adcdev.client, adcdev.channel);
	mutex_unlock(&adcdev.lock);
	return ret;
}

static inline void exynos_adc_set_channel(int channel) {
/** itop4412 一共有 4 路 ADC 接口
 * 网络标号是 XadcAIN0~XadcAIN3 
 * 开发板自带的 ADC 电路， ADC 接的是滑动变阻器 */
	if (channel < 0 || channel > 3)	{
		printk("channel value err\n");
		return;
	}
	adcdev.channel = channel;
}

/********************************************************************/

static int exynos_adc_open(struct inode *inode, struct file *filp)
{
	printk("adc opened\n");
	return 0;
}

static ssize_t exynos_adc_read(struct file *filp, char *buffer, size_t count, loff_t *offt)
{
	char str[20];
	size_t len;
	int value = exynos_adc_read_ch();
	printk("cupture adc value = %d (0x%x)\n", value, value);
	len = sprintf(str, "%d\n", value);
	if (count >= len) {
		/* 	copy_to_user() 将数据从内核空间拷贝到用户空间,一般是在驱动中 chr_drv_read()用
参数1：内核驱动中的一个buffer
参数2：应用空间到一个buffer
参数3：个数   */
		int r = copy_to_user(buffer, str, len);
		return r ? r : len;
	} else {	return -EINVAL;	}
}

static int exynos_adc_release(struct inode *inode, struct file *filp)
{
	printk("adc closed\n");
	return 0;
}

//控制io口方法函数，来源于 operations 结构体,其实就是上层系统调用传入一条命令，
//驱动识别命令，然后执行相应过程。
static long exynos_adc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int channel;
	channel = (int)arg;
/** itop4412 一共有 4 路 ADC 接口
 * 网络标号是 XadcAIN0~XadcAIN3 
 * 开发板自带的 ADC 电路， ADC 接的是滑动变阻器 */
	if (channel < 0 || channel > 3) {
		printk("channel value err\n");
		return -1;
	}
	exynos_adc_set_channel(channel);
	printk("exynos_adc_set_channel is ADC%d (XadcAIN%d)\n", channel, channel);
	return 0;
}

/********************************************************************/

/* 设备操作结构体 */
static struct file_operations adc_dev_fops = {
	owner : THIS_MODULE, /*表示本模块拥有*/
	open : exynos_adc_open,
	read : exynos_adc_read,
	release : exynos_adc_release,
	unlocked_ioctl : exynos_adc_ioctl,
};

/* MISC 设备结构体 */
static struct miscdevice misc = {
	.minor = MISC_DYNAMIC_MINOR, // 动态子设备号
	.name = DEVICE_NAME,		 // 设备名字
	.fops = &adc_dev_fops,
};

/** *************** platform 驱动 结构体 函数 ********************* **/

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int exynos_adc_probe(struct platform_device *pdev)
{
	int ret = 0;

	printk(KERN_EMERG "%s() %d\n", __FUNCTION__, __LINE__);

	mutex_init(&adcdev.lock);

	exynos_adc_set_channel(0);

	printk("adc opened\n");
	printk("%s, %d\n", __FUNCTION__, __LINE__);
	/* Register with the core ADC driver. */

	/*注册 adc*/
	adcdev.client = s3c_adc_register(pdev, NULL, NULL, 0);
	if (IS_ERR(adcdev.client))
	{
		printk("itop4412_adc: cannot register adc\n");
		ret = PTR_ERR(adcdev.client);
		goto err_mem;
	}

	/** 一般情况下会注册对应的字符设备 cdev，但是这里我们使用 MISC 设备，
	 * 所以我们不需要自己注册字符设备驱动，只需要注册misc设备驱动即可	 */
	ret = misc_register(&misc); /*注册 misc 杂项设备*/

err_mem:
	return ret;
}

/*移除驱动*/
static int exynos_adc_remove(struct platform_device *pdev)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s() %d\n", __FUNCTION__, __LINE__);

	misc_deregister(&misc); /*注销 misc 杂项设备*/

	/*注销 adc */
	s3c_adc_release(adcdev.client);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int itop4412_adc_ctl_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("itop4412_led_ctl suspend:power off!\n");
	return 0;
}

static int itop4412_adc_ctl_resume(struct platform_device *pdev)
{
	printk("itop4412_led_ctl resume:power on!\n");
	return 0;
}
#endif

/** *************** platform 驱动 结构体 ********************* **/

static struct platform_driver exynos_adc_driver = {
	.probe = exynos_adc_probe,	 /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.remove = exynos_adc_remove, /*移除驱动*/
	// .shutdown = hello_shutdown,			/*关闭驱动*/
	.suspend = itop4412_adc_ctl_suspend, /*悬挂（休眠）驱动*/
	.resume = itop4412_adc_ctl_resume,	 /*驱动恢复后要做什么*/
	.driver = {
		.name = DRIVER_NAME,  /* 名字，用于驱动和设备的匹配 */
		.owner = THIS_MODULE, /*表示本模块拥有*/
							  // .of_match_table = of_match_ptr(beep_of_match), /* 设备树匹配表 */
	},
};

/********************************************************************/
#if 0
/*驱动模块自动加载、卸载函数*/
module_platform_driver(exynos_adc_driver);
#else
/*驱动模块加载函数*/
static int __init exynos_adc_init(void)
{
	printk(KERN_INFO "%s()\n", __func__);
	return platform_driver_register(&exynos_adc_driver); /*向系统注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit exynos_adc_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_INFO "%s()\n", __func__);
	platform_driver_unregister(&exynos_adc_driver); /*向系统注销驱动*/
}

module_init(exynos_adc_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(exynos_adc_exit); /*卸载驱动时运行的函数， 如 rmmod*/
#endif
/********************************************************************/

MODULE_LICENSE("GPL"); /* 声明开源，没有内核版本限制 */
MODULE_AUTHOR("zcq");  /* 声明作者 */
MODULE_DESCRIPTION("zcq exynos adc");

/** 在内核里经常可以看到 __init, __devinit 这样的语句，
这都是在init.h中定义的宏，
gcc在编译时会将被修饰的内容放到这些宏所代表的 section，
编译器通过这些宏可以把代码优化放到合适的内存位置，以减少内存占用和提高内核效率。

__init 标记内核启动时使用的初始化代码，内核启动完成后不再需要 
__exit 标记退出代码，对于非模块无效
module_init, module_exit 宏所调用的函数，需要分别用 __init 和 __exit 来标记

pci_driver 数据结构不需要标记

probe 和 remove 函数用 __devinit 和 __devexit 来标记。
如果 remove 使用 __devexit 标记，
则在 pci_driver 结构中要用 __devexit_p(remove) 来引用 remove 函数。
如果不确定需不需要添加宏，则不要添加。

__devinit ：标记设备初始化所用的代码。
__devexit ：标记设备移除时所用的代码。 
__initdata ：标记内核启动时使用的初始化数据结构，内核启动完成后不再需要。
__devinitdata ：标记设备初始化所用的数据结构的函数。
xxx_initcall：7个级别的初始化函数。  */
