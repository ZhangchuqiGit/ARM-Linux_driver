
/* 在没有设备树的 Linux 内核下，
 * 我们需要分别编写并注册 platform_device 和 platform_driver，
 * 分别代表设备和驱动。
 * 在使用设备树的时候，设备的描述被放到了设备树中 *.dts，
 * 因此 platform_device 就不需要我们去编写了，我们只需要实现 platform_driver 即可。 */

/** 按键、鼠标、键盘、触摸屏等都属于输入(input)设备，主设备号已经固定为‘13’。
 Linux 内核为此专门做了一个叫做 input 子系统的框架来处理输入事件。输入设备本质上还是字符设备，
 只是在此基础上套上了 input 框架，用户只需要负责上报输入事件，比如按键值、坐标等信息，
 input 核心层负责处理这些事件。input 子系统的所有设备主设备号都为 INPUT_MAJOR = 13。
	在使用 input 子系统的时候我们只需要注册一个 input 设备即可，
 input_dev 结构体表示 input 设备，此结构体定义在 include/linux/input.h 文件中。
 evbit 表示输入事件类型，可选的事件类型定义在 include/uapi/linux/input.h。 
	当驱动模块加载成功以后 /dev/input 目录下会增加文件。  */

/** 
向量中断和非向量中断的判断方法
	一般一个中断号对应一个中断函数就是向量中断（独立按键） 多个中断函数共用一个中断号（矩阵键盘）

中断处理程序架构
	操作系统中会产生很多中断， 如果每一个中断都全部处理完之后再向后执行， 是不可能的， 所以就将
中断处理程序分解为上半部和下半部。
	例如给 PC 插入 U 盘会产生中断， 接收之后， 硬件会马上响应， 中断操作会很快执行上半部分， 
然后就向上半部分通知系统调用对应的驱动程序。 后面调用驱动的这个过程可以称之为下半部分。
上半部一般是和硬件紧密相关的代码， 下半部一般是耗时的一些操作。 */


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

#include <linux/input.h>
#include <linux/interrupt.h>

#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fcntl.h>

#include <linux/irq.h>
#include <linux/of_irq.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <linux/timer.h>
#include <linux/time.h>

/********************************************************************/

#define KEYINPUT_CNT   		1            		/* 设备号个数 	*/
#define KEYINPUT_NAME  		"zcq_keyinput"   	/* 设备名字 		*/

#define KEY0VALUE 			0X01        		/* KEY0按键值 	*/
#define INVAKEY 			0XFF        		/* 无效的按键值 */

#define KEY_NUM   			1            		/* 按键数量 	*/

#define timerperiod  10 		/* 定时周期,单位为 ms */

struct timer_list tml;	/* 定义一个定时器 */

/* 中断IO描述结构体 */
struct irq_keydesc {
	int gpio;                             // key 所使用的 Gled_switch PIO 编号
	unsigned int irqnum;                                /* 中断号     */
	unsigned char value;                    /* 按键对应的键值 */
	char name[10];                            /* 名字 */
	irqreturn_t (*handler)(int, void *);    /* 中断服务函数 */
};

/* keyinput设备结构体 */
struct keyinput_dev {
	dev_t devid;            /* 设备号 	 */
	struct cdev cdev;        /* cdev 	*/
	struct class *class;    /* 类 		*/
	struct device *device;    /* 设备 	 */
	struct device_node *node; /* 设备节点 */

	struct irq_keydesc irqkeydesc[KEY_NUM];    /* 按键描述数组 */
	unsigned char curkeynum;                /* 当前的按键号 */

	struct input_dev *inputdev;        /* input结构体 */
};

struct keyinput_dev keyinputdev;    /* key input设备 */

/********************************************************************/

/* @description		: 中断服务函数，开启定时器，延时10ms，
 *				  	  定时器用于按键消抖。
 * @param  irq 	: 中断号
 * @param  dev_id	: 设备结构。
 * @return 			: 中断执行结果
 */
static irqreturn_t key0_handler(int irq, void *dev_id) /*中断服务函数 (上半部)*/
{
	struct keyinput_dev *dev = (struct keyinput_dev *) dev_id;
	dev->curkeynum = 0; // 当前的按键号 KEY_0

#if 1
	/* mod_timer 函数用于修改定时值，如果定时器还没有激活的话， mod_timer 函数会激活定时器 */
	mod_timer(&tml, jiffies + msecs_to_jiffies(timerperiod));
#else
    tml.expires = jiffies + msecs_to_jiffies(timerperiod);
	add_timer(&tml); 		/*增加定时器，会激活/运行定时器, 会重新开始计时*/
#endif
	return IRQ_RETVAL(IRQ_HANDLED);
}

/* 定时器服务函数/定时器回调函数 */
void timer_callback(struct timer_list *timer)
{
	struct keyinput_dev *dev = &keyinputdev;
	unsigned char num = dev->curkeynum; // 当前的按键号 KEY_0
	struct irq_keydesc *keydesc = &dev->irqkeydesc[num];
	unsigned char value = gpio_get_value(keydesc->gpio);    /* 读取IO值 */
	if (value == 0) {                        /* 按下按键 */
		/* 上报按键值 */
		//input_event(dev->inputdev, EV_KEY, keydesc->value, 1);
		input_report_key(dev->inputdev, keydesc->value, 1); // 本质就是 input_event 函数
		/* input_report_key: 最后一个参数表示按下还是松开，1为按下，0为松开
	同样的还有一些其他的事件上报函数，这些函数如下所示：
	void input_report_rel(struct input_dev *dev, unsigned int code, int value)
	void input_report_abs(struct input_dev *dev, unsigned int code, int value)
	void input_report_ff_status(struct input_dev *dev, unsigned int code, int value)
	void input_report_switch(struct input_dev *dev, unsigned int code, int value)
	void input_mt_sync(struct input_dev *dev)  		 */
		input_sync(dev->inputdev);//上报结束 本质是上报一个同步事件
	} else {                                    /* 按键松开 */
		//input_event(dev->inputdev, EV_KEY, keydesc->value, 0);
		input_report_key(dev->inputdev, keydesc->value, 0); // 本质就是 input_event 函数
		input_sync(dev->inputdev);//上报结束 本质是上报一个同步事件
	}
	printk(KERN_EMERG "tml.expires=%ld, tml.flags=%d\r\n", tml.expires, tml.flags);
}

/*
 * @description	: 按键IO初始化
 * @param 		: 无
 * @return 		: 无
 */
int keyio_init(void)
{
	unsigned char i = 0;
	int ret = 0;

	/* 获取设备树中的属性数据 设置所使用的GPIO */
	/* 1、获取设备节点 */
#if 1
	/** 若驱动程序采用 设备树 + platform 方式, 接口： /sys/devices/platform/key */
	keyinputdev.node = of_find_node_by_path("/zcq_key"); /*设备节点*/
#else
	keyinputdev.node = pdev->dev.of_node; /*设备节点*/
#endif
	if (keyinputdev.node == NULL) {
		printk("zcq_key node not find!\r\n");
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到所使用的编号 */
	for (i = 0; i < KEY_NUM; i++) {
		keyinputdev.irqkeydesc[i].gpio = of_get_named_gpio(keyinputdev.node, "key-gpios", i);
		if (keyinputdev.irqkeydesc[i].gpio < 0) {
			printk("can't get key%d\r\n", i);
			ret = -EINVAL;
			goto fail_get_gpio;
		}
		else //打印出获取的gpio编号
			printk("key%d = %d\r\n", i, keyinputdev.irqkeydesc[i].gpio);
	}

	for (i = 0; i < KEY_NUM; i++) {
		memset(keyinputdev.irqkeydesc[i].name,
			0, sizeof(keyinputdev.irqkeydesc[i].name));    		/* 缓冲区清零 */
		sprintf(keyinputdev.irqkeydesc[i].name, "KEY%d", i); 	/* 组合名字 */

		/* 3、申请 gpio */
		//请求 IO: 申请一个 GPIO 管脚
		ret = gpio_request(keyinputdev.irqkeydesc[i].gpio, keyinputdev.irqkeydesc[i].name);
		if (ret) //如果申请失败
		{
			printk("can't request the KEY%d\r\n", i);
			ret = -EINVAL;
			goto fail_request_gpio;
		}

		/* 4、设置为输出/输入 */
		ret = gpio_direction_input(keyinputdev.irqkeydesc[i].gpio);
		if (ret < 0) {
			printk("failed to set KEY%d\r\n", i);
			ret = -EINVAL;
			goto fail_set_output;
		}
	}

	return 0;

fail_set_output:
fail_request_gpio:
fail_get_gpio:
	for (i = 0; i < KEY_NUM; i++) {
		gpio_free(keyinputdev.irqkeydesc[i].gpio);/*释放已经被占用的IO*/
	}
	return ret;
}

/********************************************************************/

/*驱动模块加载函数*/
static int __init keydriver_init(void)
{
	unsigned char i = 0;
	int ret = 0;

	printk(KERN_EMERG "%s()\n", __func__);

	/** 按键IO初始化 */
	if (keyio_init()) return -1; // 按键IO初始化

	/** 设置中断模式 */

	for (i = 0; i < KEY_NUM; i++) {
		/* 获取设备树对应的中断号 */
		keyinputdev.irqkeydesc[i].irqnum = irq_of_parse_and_map(keyinputdev.node, i);
	}

	/* 申请中断 */
	keyinputdev.irqkeydesc[0].handler = key0_handler; // 中断服务函数 (上半部)
	keyinputdev.irqkeydesc[0].value = KEY_0;

	/* 轮流调用 request_irq 函数申请中断号，
	设置中断触发模式为 IRQF_TRIGGER_FALLING 和 IRQF_TRIGGER_RISING ,
	也就是 上升沿 和 下降沿 都可以触发中断 */
	for (i = 0; i < KEY_NUM; i++) {
		/* 在 Linux 内核中要想使用某个中断是需要申请的， request_irq 函数用于申请中断，
	注册中断处理函数，request_irq 函数可能会导致睡眠，
	因此在 中断上下文 或者 其他禁止睡眠 的代码段中 不能使用。
	request_irq 函数会激活(使能)中断，所以不需要我们手动去使能中断。 */
		/** 注册中断处理函数，使能中断 */
		ret = request_irq(keyinputdev.irqkeydesc[i].irqnum, keyinputdev.irqkeydesc[i].handler,
		                  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
						  keyinputdev.irqkeydesc[i].name, &keyinputdev);
		if (ret < 0) {
			printk("irq %d request failed!\r\n", keyinputdev.irqkeydesc[i].irqnum);
			return -EFAULT;
		}
	}

	/** 创建定时器 */
	timer_setup(&tml, timer_callback, 0);	/* 设置/准备一个计时器 */ 
	// tml.function = timer_callback;

    // tml.expires = jiffies + msecs_to_jiffies(timerperiod);
	add_timer(&tml); 		/*增加定时器，会激活/运行定时器, 会重新开始计时*/

	/** 申请 input_dev */
	keyinputdev.inputdev = input_allocate_device();
	keyinputdev.inputdev->name = KEYINPUT_NAME; /*设备名字*/

	/* 初始化 input_dev，设置产生哪些事件
	#define EV_SYN 		0x00 // 同步事件
	#define EV_KEY 		0x01 // 按键事件
	#define EV_REL 		0x02 // 相对坐标事件
	#define EV_ABS 		0x03 // 绝对坐标事件
	#define EV_MSC 		0x04 // 杂项(其他)事件
	#define EV_SW 		0x05 // 开关事件
	#define EV_LED 		0x11 // LED
	#define EV_SND 		0x12 // sound(声音)
	#define EV_REP 		0x14 // 重复事件
	#define EV_FF 		0x15 // 压力事件
	#define EV_PWR 		0x16 // 电源事件
	#define EV_FF_STATUS 0x17 // 压力状态事件         
	__set_bit(EV_KEY, keyinputdev.inputdev->evbit);	设置产生按键事件 
	__set_bit(EV_REP, keyinputdev.inputdev->evbit); 重复事件，比如按下去不放开，就会一直输出信息
	 初始化input_dev，设置产生哪些按键 
	__set_bit(KEY_HOME, keyinputdev.inputdev->keybit); */

	set_bit(EV_KEY, keyinputdev.inputdev->evbit);	//设置产生按键事件 
	set_bit(EV_REP, keyinputdev.inputdev->evbit); //重复事件

#if 0
	set_bit(KEY_HOME, keyinputdev.inputdev->keybit); 
	// __set_bit(KEY_HOME, keyinputdev.inputdev->keybit);
#else
	/** 设置可允许上报的事件类型 */
	input_set_capability(keyinputdev.inputdev, EV_KEY, KEY_HOME);
#endif

	/** 注册 input 设备 */
	ret = input_register_device(keyinputdev.inputdev); // 注册 input 驱动
	if (ret) {
		printk("register input device failed!\r\n");
		return ret;
	}
	return 0;
}

/*驱动模块卸载函数*/
static void __exit keydriver_exit(void)
{
	unsigned int i = 0;
	del_timer(&tml);  			/*删除定时器*/
	for (i = 0; i < KEY_NUM; i++) {
		free_irq(keyinputdev.irqkeydesc[i].irqnum, &keyinputdev);/*释放中断*/
	}
	input_unregister_device(keyinputdev.inputdev);/*注销 input 驱动*/
	input_free_device(keyinputdev.inputdev);/*释放 input 资源*/

	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s()\n", __func__);
}

/********************************************************************/

module_init(keydriver_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(keydriver_exit); /*卸载驱动时运行的函数， 如 rmmod*/

MODULE_LICENSE("GPL"); /*声明开源的，没有内核版本限制*/
MODULE_AUTHOR("zcq");			/*声明作者*/
MODULE_DESCRIPTION("zcq key");
