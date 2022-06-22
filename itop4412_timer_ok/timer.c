
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

#include <linux/interrupt.h>
#include <linux/input.h>

#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#include <linux/timer.h>
#include <linux/time.h>

/********************************************************************/

#define timerperiod  1000 		/* 定时周期,单位为 ms */

struct timer_list tml;	/* 定义一个定时器 */

/********************************************************************/

/* 定时器服务函数/定时器回调函数 */
void timer_callback(struct timer_list *timer)
{
	printk(KERN_EMERG "tml.expires=%ld, tml.flags=%d\r\n", tml.expires, tml.flags);

#if 1
	/* mod_timer 函数用于修改定时值，如果定时器还没有激活的话， mod_timer 函数会激活定时器 */
	mod_timer(&tml, jiffies + msecs_to_jiffies(timerperiod));
	printk(KERN_EMERG "msecs_to_jiffies(%d)=%ld\r\n", 
		timerperiod, msecs_to_jiffies(timerperiod));
	// msecs_to_jiffies(1000)=100  ---- 1 * HZ ---- 1 秒
#else
    tml.expires = jiffies + msecs_to_jiffies(timerperiod);
	add_timer(&tml); 		/*增加定时器，会激活/运行定时器, 会重新开始计时*/
#endif
}

/********************************************************************/
#if 0
/*驱动模块自动加载、卸载函数*/
module_platform_driver(led_driver);
#else

/*驱动模块加载函数*/
static int __init timer_init(void)
{
	printk(KERN_EMERG "%s()\n", __func__);

	/** 创建定时器 */
	timer_setup(&tml, timer_callback, 0);	/* 设置/准备一个计时器 */ 
	// tml.function = timer_callback;

 /* 修改定时器的 expire 
mod_timer 函数用于修改定时值，如果定时器还没有激活的话， mod_timer 函数会激活/运行定时器
mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod)); 
	include/linux/jiffies.h
		全局变量 jiffies 用来记录自系统启动以来产生的节拍的总数/时钟中断次数。
	启动时，内核将该变量初始化为0，此后，每次时钟中断处理程序都会增加该变量的值。
	一秒内时钟中断的次数等于Hz，所以 jiffies 一秒内增加的值也就是Hz。
   系统运行时间以秒为单位，等于 jiffies / Hz
   比如现在需要定义一个周期为2秒的定时器，
   那么这个定时器超时时间为jiffies+(2*HZ)，因此 expires=jiffies+(2*HZ)
		●可以计算一下:	32位: 497天后溢出;  64位:天文数字 
	msecs_to_jiffies(ms)  >>   jiffies 
	Hz： 系统时钟通过 CONFIG_HZ 来设置， 范围是 100-1000； HZ 决定使用中断发生的频率。 
	如果就没有定义的话， 默认是 100， 例： 1/100 = 10ms， 说明 4412 中是 5ms 产生一次时钟中断  */
	tml.expires = jiffies + msecs_to_jiffies(timerperiod);
	add_timer(&tml); 		/*增加定时器，会激活/运行定时器, 会重新开始计时*/

	printk(KERN_EMERG "%s()\t tml.expires=%ld, tml.flags=%d\r\n", __func__, 
		tml.expires, tml.flags);
	return 0;
}

/*驱动模块卸载函数*/
static void __exit timer_exit(void) 
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s()\n", __func__);

	del_timer(&tml);  			/*删除定时器*/
}

#endif
/********************************************************************/

module_init(timer_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(timer_exit); /*卸载驱动时运行的函数， 如 rmmod*/

MODULE_LICENSE("GPL"); 	/* 声明开源，没有内核版本限制*/
MODULE_AUTHOR("zcq");	/*声明作者*/
MODULE_DESCRIPTION("zcq timer");
