#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/of_irq.h>
#include <linux/irq.h>

#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#include <linux/interrupt.h>
#include <linux/input.h>

/* 在没有设备树的 Linux 内核下，我们需要分别编写并注册 platform_device 和 platform_driver，
 * 分别代表设备和驱动。在使用设备树的时候，设备的描述被放到了设备树中，
 * 因此 platform_device 就不需要我们去编写了，我们只需要实现 platform_driver 即可。 */

/* MISC 杂项驱动
 	所有的 MISC 设备驱动的主设备号都为 10，不同的设备使用不同的从设备号。
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

/** 按键、鼠标、键盘、触摸屏等都属于输入(input)设备，
 Linux 内核为此专门做了一个叫做 input 子系统的框架来处理输入事件。输入设备本质上还是字符设备，
 只是在此基础上套上了 input 框架，用户只需要负责上报输入事件，比如按键值、坐标等信息，
 input 核心层负责处理这些事件。input 子系统的所有设备主设备号都为 INPUT_MAJOR = 13。
	在使用 input 子系统的时候我们只需要注册一个 input 设备即可，
 input_dev 结构体表示 input 设备，此结构体定义在 include/linux/input.h 文件中。
 evbit 表示输入事件类型，可选的事件类型定义在 include/uapi/linux/input.h。
input_event
	当驱动模块加载成功以后 /dev/input 目录下会增加文件。  */

#define KEYINPUT_CNT        1            /* 设备号个数 	*/
#define KEYINPUT_NAME        "zcq_keyinput"    /* 设备名字 		*/

#define KEY0VALUE            0X01        /* KEY0按键值 	*/
#define INVAKEY                0XFF        /* 无效的按键值 */

#define KEY_NUM                1            /* 按键数量 	*/

/* 寄存器物理地址：设备树方式不需要 */

/* 映射后的寄存器虚拟地址指针：princtl 方式不需要 */

/* 中断IO描述结构体 */
struct irq_keydesc {
	int gpio;                             // key 所使用的 Gled_switch PIO 编号
	int irqnum;                                /* 中断号     */
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

	struct timer_list timer;/* 定义一个定时器*/
	struct irq_keydesc irqkeydesc[KEY_NUM];    /* 按键描述数组 */
	unsigned char curkeynum;                /* 当前的按键号 */

	struct input_dev *inputdev;        /* input结构体 */
};

struct keyinput_dev keyinputdev;    /* key input设备 */

/** @description		: 中断服务函数，开启定时器，延时10ms，
 *				  	  定时器用于按键消抖。
 * @param  irq 	: 中断号
 * @param  dev_id	: 设备结构。
 * @return 			: 中断执行结果
 */
static irqreturn_t key0_handler(int irq, void *dev_id)
{
	struct keyinput_dev *dev = (struct keyinput_dev *) dev_id;
	dev->timer.data = (volatile long) dev_id;

	dev->curkeynum = 0; // 当前的按键号 KEY_0

	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));    /* 10ms定时 */
	return IRQ_RETVAL(IRQ_HANDLED);
}

/* @description	: 定时器服务函数，用于按键消抖，定时器到了以后
 *				  再次读取按键值，如果按键还是处于按下状态就表示按键有效。
 * @param - arg	: 设备结构变量
 * @return 		: 无
 */
void timer_function(unsigned long arg)
{
	struct keyinput_dev *dev = (struct keyinput_dev *) arg;
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
}

/*
 * @description	: 按键IO初始化
 * @param 		: 无
 * @return 		: 无
 */
static int keyio_init(void)
{
	unsigned char i = 0;
	char name[10];
	int ret = 0;

	/* 获取设备树中的属性数据 设置所使用的GPIO */
	/* 1、获取设备节点 */
	keyinputdev.node = of_find_node_by_path("/key");
	if (keyinputdev.node == NULL) {
		printk("key node not find!\r\n");
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到 key 所使用的编号 */
	for (i = 0; i < KEY_NUM; i++) {
		keyinputdev.irqkeydesc[i].gpio = of_get_named_gpio(keyinputdev.node, "key-gpio", i);
		if (keyinputdev.irqkeydesc[i].gpio < 0) {
			printk("can't get key%d\r\n", i);
		}
	}

	/* 初始化key所使用的IO，并且设置成中断模式 */
	for (i = 0; i < KEY_NUM; i++) {
		memset(keyinputdev.irqkeydesc[i].name, 0, sizeof(name));    /* 缓冲区清零 */
		sprintf(keyinputdev.irqkeydesc[i].name, "KEY%d", i);        /* 组合名字 */

		/* 初始化 key 所使用的 IO */
		gpio_request(keyinputdev.irqkeydesc[i].gpio, name);

		/* 3、设置为输入 */
		gpio_direction_input(keyinputdev.irqkeydesc[i].gpio);

		keyinputdev.irqkeydesc[i].irqnum = (int) irq_of_parse_and_map(keyinputdev.node, i);
		// 获取设备树对应的中断号
	}

	/* 申请中断 */
	keyinputdev.irqkeydesc[0].handler = key0_handler; // 中断服务函数 (上半部)
	keyinputdev.irqkeydesc[0].value = KEY_0;

	/* 轮流调用 request_irq 函数申请中断号，
	设置中断触发模式为 IRQF_TRIGGER_FALLING 和 IRQF_TRIGGER_RISING，
	也就是 上升沿 和 下降沿 都可以触发中断 */
	for (i = 0; i < KEY_NUM; i++) {
		/* 在 Linux 内核中要想使用某个中断是需要申请的， request_irq 函数用于申请中断，
	注册中断处理函数，request_irq 函数可能会导致睡眠，
	因此在 中断上下文 或者 其他禁止睡眠 的代码段中 不能使用。
	request_irq 函数会激活(使能)中断，所以不需要我们手动去使能中断。 */
		/** 注册中断处理函数，使能中断 */
		ret = request_irq(keyinputdev.irqkeydesc[i].irqnum, keyinputdev.irqkeydesc[i].handler,
		                  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, keyinputdev.irqkeydesc[i].name,
		                  &keyinputdev);
		if (ret < 0) {
			printk("irq %d request failed!\r\n", keyinputdev.irqkeydesc[i].irqnum);
			return -EFAULT;
		}
	}

	/** 创建定时器 */
	init_timer(&keyinputdev.timer);// 初始化定时器
	keyinputdev.timer.function = timer_function;// 设置定时器的定时处理函数

	/** 申请 input_dev */
	keyinputdev.inputdev = input_allocate_device();
	keyinputdev.inputdev->name = KEYINPUT_NAME; /*设备名字*/
#if 0
	/* 初始化input_dev，设置产生哪些事件
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
	#define EV_FF_STATUS 0x17 // 压力状态事件          */
	__set_bit(EV_KEY, keyinputdev.inputdev->evbit);	/* 设置产生按键事件 */
	__set_bit(EV_REP, keyinputdev.inputdev->evbit);	/* 重复事件，比如按下去不放开，就会一直输出信息 */

	/* 初始化input_dev，设置产生哪些按键 */
	__set_bit(KEY_0, keyinputdev.inputdev->keybit);
#endif
#if 0
	keyinputdev.inputdev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	keyinputdev.inputdev->keybit[BIT_WORD(KEY_0)] |= BIT_MASK(KEY_0);
#endif
	keyinputdev.inputdev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	input_set_capability(keyinputdev.inputdev, EV_KEY, KEY_0);

	/** 注册输入设备 */
	ret = input_register_device(keyinputdev.inputdev); // 注册 input 驱动
	if (ret) {
		printk("register input device failed!\r\n");
		return ret;
	}
	return 0;
}

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init

keyinput_init(void)
{
	keyio_init();
	return 0;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit keyinput_exit(void)
{
	unsigned int i = 0;
	/* 删除定时器 */
	del_timer_sync(&keyinputdev.timer);    /* 删除定时器 */

	/* 释放中断 */
	for (i = 0; i < KEY_NUM; i++) {
		free_irq(keyinputdev.irqkeydesc[i].irqnum, &keyinputdev);
	}

	/** 释放input_dev */
	input_unregister_device(keyinputdev.inputdev); // 注销 input 驱动
	input_free_device(keyinputdev.inputdev);
}

module_init(keyinput_init);
module_exit(keyinput_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
