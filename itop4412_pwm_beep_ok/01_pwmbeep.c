
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

/* 原子操作的函数头文件
#include <asm/atomic.h>
#include <asm/types.h>			*/

#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/input.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/property.h>
#include <linux/pwm.h>
#include <linux/workqueue.h>

#include <linux/timer.h>
#include <linux/time.h>

#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/acpi.h>
#include <linux/clk.h>

#include <linux/leds.h>

/********************************************************************/

#define DRIVER_NAME "zcq_pwmbeer_driver" 		/* 驱动名字 */

void __iomem *pwm_reg_base = NULL;
static struct clk *my_pwm_clock;

/** *************** platform 驱动 结构体 函数 ********************* **/

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int pwmbeep_probe(struct platform_device *pdev)
{
    unsigned int tmp;
    struct resource *mem_res;

    mem_res = platform_get_resource(pdev, IORESOURCE_MEM/*获取DTS中reg的信息*/, 0);
    if (mem_res == NULL) {
        dev_err(&pdev->dev, "Unable to get PWM MEM resource\n");
        return -ENXIO;
    }

    //获取DTS中clock的信息
    my_pwm_clock = devm_clk_get(&pdev->dev, "timers");
    if (IS_ERR(my_pwm_clock)) {
        dev_err(&pdev->dev, "Unable to acquire clock 'my_pwm_clock'\n");
        return 0;
    }

	/*使能时钟源，100MHz*/
    if (clk_prepare_enable(my_pwm_clock)) {
        dev_err(&pdev->dev, "Couldn't enable clock 'my_pwm_clock'\n");
        return 0;
    }

    //将获取到的reg的信息，进行ioremap，为后续操作PWM电路做准备
    pwm_reg_base = devm_ioremap_resource(&pdev->dev, mem_res);
    if (IS_ERR(pwm_reg_base)) {
        dev_err(&pdev->dev, "Couldn't ioremap resource\n");
        clk_disable_unprepare(my_pwm_clock);
        return 0;
    }

    /*将分频器设置为0x7c，也就是124，可将时钟源分频为800KHz*/
    /*input clock = 100MHz / (124+1) = 800KHz*/
    writel(0x7c, pwm_reg_base + 0x0);

    /*将分频器设置为0x30，可将时钟源分频为100KHz */
    /*input clock = 800KHz / 8 = 100KHz*/
    writel(0x3 << 0, pwm_reg_base + 0x4);

    /*装载计数寄存器器100，即为从100开始递减，
	等减到和比较寄存器的值一样时翻转电平，减到0即为一个周期。
    结合时钟源，就可以计算PWM的输出波形的周期，
	10微妙计数一次，一个周期计数100次，PWM输出的波形周期即为1毫秒 */
    /*time 1 count buffer: 100=0x64*/
    // writel(0x64, pwm_reg_base + 0x18);
    // writel(100, pwm_reg_base + 0x18);
    writel(1000, pwm_reg_base + 0x18);

    /*装载比较寄存器80，计数器从100开始递减，减到80翻转电平。*/
    /*time 1 compare buffer: 80=0x50*/
    // writel(0x50, pwm_reg_base + 0x1c);
    // writel(50, pwm_reg_base + 0x1c);
    writel(1, pwm_reg_base + 0x1c);

    /**将TCON[9]置为1，把计数寄存器和比较寄存器的值更新到计数器和比较器里面，
    再清零（不清零PWM无法工作）*/
    writel(0, pwm_reg_base + 0x8);
    /*update timer1 count & compare*/
    tmp = readl(pwm_reg_base + 0x8);
    tmp |= (1 << 9);	/* pdates TCNTB1 and TCMPB1 */
    writel(tmp, pwm_reg_base + 0x8);
    /*need clear*/
    tmp &= ~(1 << 9);
    writel(tmp, pwm_reg_base + 0x8);

    /*使能自动装载和电平翻转*/
    /*auto-load and inverter-on */
    tmp = readl(pwm_reg_base + 0x8);
    tmp |= (1 << 10) | (1 << 11);
    writel(tmp, pwm_reg_base + 0x8);

    /*enable timer1*/
    tmp = readl(pwm_reg_base + 0x8);
    tmp |= (0x1 << 8);
    writel(tmp, pwm_reg_base + 0x8);	

	printk(KERN_INFO "pwmbeep_probe()\n");
	return 0;
}

/*移除驱动*/
static int pwmbeep_remove(struct platform_device *dev)
{
	// gpio_set_value(miscbeep.beep_gpio, 0); /* 卸载驱动的时候关闭LED */
	// gpio_free(miscbeep.beep_gpio); 			/*释放已经被占用的 IO*/
	
	clk_disable_unprepare(my_pwm_clock);
	return 0;
}

/** *************** 设备树 *.dts 匹配列表 ********************* **/

static const struct of_device_id beep_of_match[] = {
	{.compatible = "zcq,pwm"}, /* 兼容属性 */
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

MODULE_LICENSE("GPL"); 	/* 声明开源，没有内核版本限制*/
MODULE_AUTHOR("zcq");/*声明作者*/
MODULE_DESCRIPTION("zcq pwmbeep");
