/**
adc_demo @126C0000
{
    compatible = "tiny4412,adc_demo";
    reg = <0x126C 0x20>;
    clocks = <&clock CLK_TSADC>;
    clock - names = "timers";
    interrupt - parent = <&combiner>;
    interrupts = <10 3>;
}*/

/** 工业场合里面也有大量的模拟量和数字量之间的转换，也就是我们常说的 ADC 和 DAC。
而且随着手机、物联网、工业物联网和可穿戴设备的爆发，传感器的需求只持续增强。比如手
机或者手环里面的加速度计、光传感器、陀螺仪、气压计、磁力计等，这些传感器本质上都是
ADC，大家注意查看这些传感器的手册，会发现他们内部都会有个 ADC，
传感器对外提供 IIC 或者 SPI 接口， SOC 可以通过 IIC 或者 SPI 接口来获取到传感器内部的 ADC 数值，
从而得到想要测量的结果。Linux 内核为了管理这些日益增多的 ADC 类传感器，特地推出了 IIO 子系统  **/

/********************************************************************/

/** ADC使用的是SOC自带的功能，一般SOC厂家已经把相应的驱动代码写好，
	我们只需要在设备树中使能该功能则可。在进行ADC读操作时，
	只需要了解对IIO子系统的使用操作，即可完成ADC的读取

	编译并烧写内核，启动后即可在终端下运行以下命令来读取 ADC0 的值
	数据采集的过程中，旋转电位器的旋钮，改变电位器的电阻分压，就会改变转换后的结果。
	cat /sys/devices/platform/126c0000.adc/iio:device0/in_voltage0_raw
	cat /sys/bus/iio/devices/iio\:device0/in_voltage0_raw
	/dev/iio:device0

	itop4412 一共有 4 路 ADC 接口
	* 网络标号是 XadcAIN0~XadcAIN3 
	* 开发板自带的 ADC 电路， ADC 接的是滑动变阻器
	Power Supply Voltage: 1.8V (Typ.), 1.0V (Typ., Digital I/O Interface)
	Analog Input Range: 0 ~ 1.8V 
	Exynos 4412 has two ADC blocks, ADC_CFG[16] setting :
		General ADC : 0x126C_0000
		MTCADC_ISP :  0x1215_0000  
	sec_exynos4412_users manual_ver.1.00.00.pdf  
	57 ADC 57.7	p2770

	/proc/device-tree/adc@126C0000 */

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

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/interrupt.h>

/********************************************************************/

DECLARE_WAIT_QUEUE_HEAD(wait);

#define DEVICE_NAME "adc_demo_device"
#define DRIVER_NAME "adc_demo_driver"

typedef struct
{
	struct mutex lock;
	struct s3c_adc_client *client;
	int channel;
} ADC_DEV;
static ADC_DEV adcdev;

unsigned int irqnum; /* 中断号     */
struct clk *base_clk = NULL;

/********************************************************************/

static inline int exynos_adc_read_ch(void)
{
	int ret = mutex_lock_interruptible(&adcdev.lock);
	if (ret < 0)
		return ret;
	ret = s3c_adc_read(adcdev.client, adcdev.channel);
	mutex_unlock(&adcdev.lock);
	return ret;
}

static inline void exynos_adc_set_channel(int channel)
{
	/** itop4412 一共有 4 路 ADC 接口
 * 网络标号是 XadcAIN0~XadcAIN3 
 * 开发板自带的 ADC 电路， ADC 接的是滑动变阻器 */
	if (channel < 0 || channel > 3)
	{
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

static ssize_t exynos_adc_read(struct file *filp, char __user *buffer, size_t count, loff_t *offt)
{
	char str[20];
	size_t len;
	int value = exynos_adc_read_ch();
	printk("cupture adc value = %d (0x%x)\n", value, value);
	len = sprintf(str, "%d\n", value);
	if (count >= len)
	{
		/* 	copy_to_user() 将数据从内核空间拷贝到用户空间,一般是在驱动中 chr_drv_read()用
参数1：内核驱动中的一个buffer
参数2：应用空间到一个buffer
参数3：个数   */
		int r = copy_to_user(buffer, str, len);
		return r ? r : len;
	}
	else
	{
		return -EINVAL;
	}
	return count;
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
	if (channel < 0 || channel > 3)
	{
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
static struct miscdevice adc_misc = {
	.minor = MISC_DYNAMIC_MINOR, // 动态子设备号
	.name = DEVICE_NAME,		 // 设备名字
	.fops = &adc_dev_fops,
};

/********************************************************************/

static irqreturn_t adc_demo_isr(int irq, void *dev_id) /*中断服务函数 (上半部)*/
{
    printk("enter irq now to wake up\n");
    wake_up(&wait);

    /* clear irq */
    regs_base->CLRINTADC = 1;

	return IRQ_RETVAL(IRQ_HANDLED);
}

/** *************** platform 驱动 结构体 函数 ********************* **/

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int exynos_adc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource	*mem = NULL;
	void __iomem *regs_base = NULL;
    struct device *dev = &pdev->dev;

	printk(KERN_EMERG "%s() %d\n", __FUNCTION__, __LINE__);

	/*获得ADC寄存器地址*/
	mem = platform_get_resource(pdev, IORESOURCE_MEM/*获取DTS中reg的信息*/, 0);
    if (mem == NULL) {
        printk("platform_get_resource error\n");
        return -EINVAL;
    }
    printk("mem: %x\n", (unsigned int)mem->start);
	//将获取到 DTS 的 reg 的信息，进行 ioremap 地址映射，为后续操作做准备
	regs_base = devm_ioremap_resource(&pdev->dev, mem);
	if (regs_base == NULL) {
		printk("devm_ioremap_resource error\n");
        return -EINVAL;
    }

	/********************************************************************/
	/*获取 DTS 中 clock 的信息*/
    base_clk = devm_clk_get(&pdev->dev, "adc");
    if (IS_ERR(base_clk))    {
        dev_err(dev, "failed to get timer base clk\n");
        return PTR_ERR(base_clk);
    }

    ret = clk_prepare_enable(base_clk);
    if (ret < 0)    {
        dev_err(dev, "failed to enable base clock\n");
        goto err_clk;
    }

	/********************************************************************/
	/* 获取 DTS 中 interrupts 的信息(中断号) */
    irqnum = platform_get_irq(pdev, 0);
    if (irqnum < 0)    {
        dev_err(&pdev->dev, "no irq resource?\n");
        goto err_clk;
    }

	/* 在 Linux 内核中要想使用某个中断是需要申请的， request_irq 函数用于申请中断，
	注册中断处理函数，request_irq 函数可能会导致睡眠，
	因此在 中断上下文 或者 其他禁止睡眠 的代码段中 不能使用。
	request_irq 函数会激活(使能)中断，所以不需要我们手动去使能中断。 */
	/** 注册中断处理函数，使能中断(进程) */
    ret = request_irq(irqnum, adc_demo_isr, IRQF_TRIGGER_NONE, "adc", NULL);
    if (ret < 0)    {
        dev_err(dev, "failed to request_irq\n");
        goto err_clk;
    }
	/********************************************************************/

	/** 一般情况下会注册对应的字符设备 cdev，但是这里我们使用 MISC 设备，
	 * 所以我们不需要自己注册字符设备驱动，只需要注册misc设备驱动即可	 */
	ret = misc_register(&adc_misc); /*注册 misc 杂项设备*/
	if (ret < 0) {
		printk("misc device register failed!\r\n");
		return -EFAULT;
	}

	return 0;
err_clk:
    // clk_disable(base_clk);
    // clk_unprepare(base_clk);
	clk_disable_unprepare(base_clk);
	return ret;
}

/*移除驱动*/
static int exynos_adc_remove(struct platform_device *pdev)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s() %d\n", __FUNCTION__, __LINE__);

	misc_deregister(&adc_misc); /*注销 misc 杂项设备*/
	clk_disable_unprepare(base_clk);
    free_irq(irqnum, NULL);/*释放中断*/
	return 0;
}

// #ifdef CONFIG_PM_SLEEP
/*悬挂（休眠）驱动*/
static int exynos_adc_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("exynos_adc_suspend !\n");
	return 0;
}

/*驱动恢复后要做什么*/
static int exynos_adc_resume(struct platform_device *pdev)
{
	printk("exynos_adc_resume !\n");
	return 0;
}
// #endif

/** *************** 设备树 *.dts 匹配列表 ********************* **/

static const struct of_device_id adc_of_match[] = {
	{.compatible = "itop4412,adc_demo"}, /* 兼容属性 */
	{},
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE(of, adc_of_match);

/** *************** platform 驱动 结构体 ********************* **/

static SIMPLE_DEV_PM_OPS(exynos_adc_pm_ops,
						 exynos_adc_suspend, /*悬挂（休眠）驱动*/
						 exynos_adc_resume	 /*驱动恢复后要做什么*/
);

static struct platform_driver exynos_adc_driver = {
	.probe = exynos_adc_probe,	 /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.remove = exynos_adc_remove, /*移除驱动*/
	// .shutdown = hello_shutdown,			/*关闭驱动*/
	.driver = {
		.name = DRIVER_NAME,  /* 名字，用于驱动和设备的匹配 */
		/*.owner = THIS_MODULE, 表示本模块拥有*/
		.pm = &exynos_adc_pm_ops,
		.of_match_table = adc_of_match, /* 设备树匹配表 */
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
	int ret = 0;
	printk(KERN_INFO "%s()\n", __func__);
	ret = platform_driver_register(&exynos_adc_driver); /*向系统注册驱动*/
	if (ret) printk(KERN_ERR "adc demo: probe faiad: %d\n", ret);
	return ret;
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

MODULE_LICENSE("GPL v2"); /* 声明开源，没有内核版本限制 */
MODULE_AUTHOR("zcq");  /* 声明作者 */
MODULE_DESCRIPTION("zcq exynos adc");
