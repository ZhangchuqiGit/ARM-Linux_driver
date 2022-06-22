
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

#include <linux/input.h>
#include <linux/regulator/consumer.h>
#include <linux/kernel.h>
#include <linux/property.h>
#include <linux/pwm.h>
#include <linux/workqueue.h>

#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/backlight.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <linux/spinlock.h>		/** 自旋锁 */

/********************************************************************/

#define DRIVER_NAME "zcq_pwmbeep_dri"		 /*驱动名字*/
#define DEVICE_NAME "zcq_pwmbeep_dev"		 /*设备名字 兼容属性 compatible*/
#define HZ_TO_NANOSECONDS(x) (1000000000UL / (x))

struct pwm_beeper {
	struct input_dev *input;
	struct pwm_device *pwm_dev;		/*pwm 设备资源*/
	struct regulator *amplifier;	/*放大器*/
	struct work_struct work;
	unsigned long period;			/*pwm周期时间，单位为ns*/ 
	unsigned int bell_frequency;	/*设置频率:HZ*/
	bool suspended;
	bool amplifier_on;
};

/********************************************************************/

//设置频率:HZ
static void __maybe_unused pwm_set_freq(struct pwm_beeper *beeper,
										int rate /* 0% ~ 100% */ ,
										unsigned long period_ns /*pwm周期时间，单位为ns*/ )
{
	int duty_ns = 0; 	/*pwm占空比时间，单位为ns*/
	if (rate < 0 || rate > 100) rate = 50;
	// duty_ns = period_ns / 2; 			/*pwm占空比 50% */
	duty_ns = period_ns / 100 * rate; 		/*pwm占空比时间，单位为ns*/
	pwm_config(beeper->pwm_dev, duty_ns, period_ns);	/* 配置 PWM */
	pwm_enable(beeper->pwm_dev);
}

static int pwm_beeper_on(struct pwm_beeper *beeper, unsigned long period)
{
	struct pwm_state state;
	int error;
#if 1
	pwm_get_state(beeper->pwm_dev, &state);//查询PWM状态
	state.enabled = true;
	state.period = period;	/*pwm周期时间，单位为ns*/ 
	pwm_set_relative_duty_cycle(&state, 50, 100); // 设置 pwm 占空比: 50% = 50/100
	error = pwm_apply_state(beeper->pwm_dev, &state);//应用PWM状态
	if (error) return error;
#else
	pwm_set_freq(beeper, 50, period);
#endif
	if (!beeper->amplifier_on) {
		error = regulator_enable(beeper->amplifier);
		if (error) {
			pwm_disable(beeper->pwm_dev);
			return error;
		}
		beeper->amplifier_on = true;
	}
	return 0;
}

static void pwm_beeper_off(struct pwm_beeper *beeper)
{
	if (beeper->amplifier_on) {
		regulator_disable(beeper->amplifier);
		beeper->amplifier_on = false;
	}
	pwm_disable(beeper->pwm_dev);
}

static void pwm_beeper_stop(struct pwm_beeper *beeper)
{
	cancel_work_sync(&beeper->work);
	pwm_beeper_off(beeper);
}

/********************************************************************/

/*编写要提交到工作队列中的函数 (延时调度的一个自定义函数)*/
static void pwm_beeper_work(struct work_struct *work)
{
	/*宏函数 container_of(ptr,type,member) 
	ptr,type,member 分别代表指针、类型、成员
	通过一个结构变量中一个成员的地址找到这个 结构体变量 的首地址
	原理： 已知结构体 type 的成员 member 的地址 ptr，求解结构体 type 的起始地址
	实现：
	1. 判断 ptr 与 member 是否为同意类型
    2. 计算 size 大小， size = &((type *)0)->member
	而 &((type *)0)->member 的作用就是求 member 到结构体 type 起始地址的字节数大小(就是 size);
	在这里 0 被强制转化为 type * 型， 它的作用就是作为指向该结构体起始地址的指针;
	结构体的起始地址 = (type *)((char *)ptr - size)   (注：强转为该结构体指针)	 */
	struct pwm_beeper *beeper = container_of(work, struct pwm_beeper, work);
	/*确保所生成的代码对向其传递的参数的值确实只被访问过一次*/
	unsigned long period = READ_ONCE(beeper->period);

	if (period) pwm_beeper_on(beeper, period);
	else pwm_beeper_off(beeper);
}

/********************************************************************/

static int pwm_beeper_event(struct input_dev *input,
							unsigned int type, unsigned int code, int value)
{
	struct pwm_beeper *beeper = input_get_drvdata(input);/*获取私有数据*/
	if (type != EV_SND || value < 0) return -EINVAL;
	switch (code) {
		case SND_BELL:
			break;
		case SND_TONE:
			break;
		default:
			return -EINVAL;
	}
	if (value == 0) beeper->period = 0;
	else beeper->period = HZ_TO_NANOSECONDS(beeper->bell_frequency);

	/*将工作结构体变量添加到共享工作队列,工作完成后会自动从队列中删除*/
	if (!beeper->suspended) schedule_work(&beeper->work);
	return 0;
}

static void pwm_beeper_close(struct input_dev *input)
{
	struct pwm_beeper *beeper = input_get_drvdata(input);/*获取私有数据*/
	pwm_beeper_stop(beeper);
}

/** *************** platform 驱动 结构体 函数 ********************* **/

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int pwm_beeper_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pwm_beeper *beeper;
	struct pwm_state state;
	u32 bell_frequency = 100;
	int err = -EINVAL;

	/*分配对齐的内存并清0*/
	beeper = devm_kzalloc(dev, sizeof(*beeper), GFP_KERNEL);
	if (!beeper) return -ENOMEM;

	/********************************************************************/

	/** 使用PWM
	传统用户可以使用 pwm_request()请求PWM设备，并在使用 pwm_free()后将其释放。
	新用户应使用 pwm_get()函数 请求PWM设备, 并将消费者设备（使用者）名称传递给它。 
	pwm_put()用于 释放PWM设备。
	这些函数的变体, devm_pwm_get()和 devm_pwm_put()也存在 */
	beeper->pwm_dev = devm_pwm_get(dev, NULL); /*请求PWM设备*/
	if (IS_ERR(beeper->pwm_dev)) {
		error = PTR_ERR(beeper->pwm_dev);
		if (error != -EPROBE_DEFER)
			dev_err(dev, "Failed to request PWM device: %d\n", error);
		return error;
	}

	/** Sync up PWM state and ensure it is off. */
	pwm_init_state(beeper->pwm_dev, &state);
	state.enabled = false;
	error = pwm_apply_state(beeper->pwm_dev, &state);//应用PWM状态
	if (error) {
		dev_err(dev, "failed to apply initial PWM state: %d\n", error);
		return error;
	}

	/********************************************************************/

	beeper->amplifier = devm_regulator_get(dev, "amp");
	if (IS_ERR(beeper->amplifier)) {
		error = PTR_ERR(beeper->amplifier);
		if (error != -EPROBE_DEFER)
			dev_err(dev, "Failed to get 'amp' regulator: %d\n", error);
		return error;
	}

	error = device_property_read_u32(dev, "beeper-hz", &bell_frequency);
	if (error) {
		dev_dbg(dev,
				"failed to parse 'beeper-hz' property, using default: %uHz\n",
				bell_frequency);
	}
	beeper->bell_frequency = bell_frequency; //设置频率:HZ

	/********************************************************************/

	/* 	初始化工作队列 _work 
	编写要提交到工作队列中的函数 (延时调度的一个自定义函数) _func
	注，调用完毕后系统会释放此函数，所以如果想再次执行的话，
	就再次调用 schedule_work() 即可。
	另外，内核必须挂载文件系统才可以使用工作队列。
	可理解：工作队列也属于调度，如果内核挂了，他就不调度了，当然就不能用工作队列了。 */
	INIT_WORK(&beeper->work, pwm_beeper_work);

	/********************************************************************/

	/** 申请 input_dev 资源 */
	beeper->input = devm_input_allocate_device(dev);
	if (!beeper->input) {
		dev_err(dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	/** event设备信息： cat /proc/bus/input/devices 
	I: Bus=0019 Vendor=001f Product=0001 Version=0100
	N: Name=DEVICE_NAME
	P: Phys="pwm/input0"         	*/
	beeper->input->id.bustype = BUS_HOST;
	beeper->input->id.vendor = 0x001f;
	beeper->input->id.product = 0x0001;
	beeper->input->id.version = 0x0100;

	beeper->input->name = DEVICE_NAME;  /*设备名字 兼容属性 compatible*/
	beeper->input->phys = "pwm/input0";

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
	__set_bit(KEY_0, keyinputdev.inputdev->keybit); */
	/** 设置可允许上报的事件类型 */
	input_set_capability(beeper->input, EV_SND, SND_TONE);
	input_set_capability(beeper->input, EV_SND, SND_BELL);

	beeper->input->event = pwm_beeper_event;
	beeper->input->close = pwm_beeper_close;

	/** 注册 input 设备 */
	error = input_register_device(beeper->input);
	if (error) {
		input_free_device(beeper->input);/* 释放 input_dev 资源 */
		dev_err(dev, "Failed to register input device: %d\n", error);
		return error;
	}

	/********************************************************************/

	/*将保存为设备的私有数据，可通过 input_get_drvdata 获取数据*/
	input_set_drvdata(beeper->input, beeper);

	/*将保存为设备的私有数据，可通过 platform_get_drvdata 获取数据*/
	platform_set_drvdata(pdev, beeper);
	return 0;
}

/*移除驱动*/
static int pwm_beeper_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pwm_beeper *beeper = platform_get_drvdata(pdev);
	pwm_beeper_stop(beeper);

	devm_pwm_put(dev, beeper->pwm_dev); /*释放PWM设备*/

	input_unregister_device(beeper->input);/*注销 input 驱动*/
	input_free_device(beeper->input);/*释放 input_dev 资源 */
	// if (beeper->input) kfree(ts->input_dev);/* 释放内存 */
	return 0;
}

/*悬挂（休眠）驱动*/
static int __maybe_unused pwm_beeper_suspend(struct device *dev)
{
	struct pwm_beeper *beeper = dev_get_drvdata(dev);/*获取私有数据*/

	/* 中断里面可以使用自旋锁，在获取锁之前一定要先禁止本地中断(也就是本 CPU 中断，
对于多核 SOC 来说会有多个 CPU 核)，否则可能导致锁死现象的发生。
	中断处理 自旋锁 API 函数 					描述
	void spin_lock_irq(spinlock_t *lock) 	禁止本地中断，并获取自旋锁。
	void spin_unlock_irq(spinlock_t *lock) 	激活本地中断，并释放自旋锁。	 */
	spin_lock_irq(&beeper->input->event_lock);
	beeper->suspended = true;
	spin_unlock_irq(&beeper->input->event_lock);

	pwm_beeper_stop(beeper);
	return 0;
}

/*驱动恢复后要做什么*/
static int __maybe_unused pwm_beeper_resume(struct device *dev)
{
	struct pwm_beeper *beeper = dev_get_drvdata(dev);/*获取私有数据*/

	spin_lock_irq(&beeper->input->event_lock);
	beeper->suspended = false;
	spin_unlock_irq(&beeper->input->event_lock);

	/*将工作结构体变量添加到共享工作队列,工作完成后会自动从队列中删除*/
	schedule_work(&beeper->work);
	return 0;
}

/** *************** 设备树 *.dts 匹配列表 ********************* **/

#ifdef CONFIG_OF
static const struct of_device_id pwm_beeper_match[] = {
	{.compatible = DEVICE_NAME },  /*设备名字 兼容属性 compatible*/
	{ /* sentinel */ },
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE(of, pwm_beeper_match);
#endif

/** *************** platform 驱动 结构体 ********************* **/

static SIMPLE_DEV_PM_OPS(pwm_beeper_pm_ops,
						 pwm_beeper_suspend, /*悬挂（休眠）驱动*/
						 pwm_beeper_resume	 /*驱动恢复后要做什么*/
);

static struct platform_driver pwm_beeper_driver = {
	.probe = pwm_beeper_probe, /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.remove = pwm_beeper_remove, /*移除驱动*/
	.driver = {
		.name = DRIVER_NAME, /* 驱动名字 */
		.owner = THIS_MODULE, /*表示本模块拥有*/
		.pm = &pwm_beeper_pm_ops,
		.of_match_table = of_match_ptr(pwm_beeper_match), /* 设备树匹配表 */
	},
};

/********************************************************************/
#if 0
/*驱动模块自动加载、卸载函数*/
module_platform_driver(pwm_beeper_driver);
#else
/*驱动模块加载函数*/
static int __init miscbeep_init(void)
{
	printk(KERN_EMERG "%s()\n", __func__);
	return platform_driver_register(&pwm_beeper_driver); /*向系统注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit miscbeep_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s()\n", __func__);
	platform_driver_unregister(&pwm_beeper_driver); /*向系统注销驱动*/
}

module_init(miscbeep_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(miscbeep_exit); /*卸载驱动时运行的函数， 如 rmmod*/
#endif
/********************************************************************/

MODULE_LICENSE("GPL"); /* 声明开源，没有内核版本限制 */
MODULE_AUTHOR("zcq");  /* 声明作者 */
// MODULE_ALIAS("platform:zcq_pwm-beeper");
MODULE_DESCRIPTION("PWM beeper driver");

/********************************************************************/

/* 工作队列 基本流程 
	在许多情况下，设备驱动程序不需要有自己的工作队列。
	如果我们只是偶尔需要向队列中提交任务，则一种更简单、更有效的办法是使用内核提供的共享的默认工作队列。
	但是，如果我们使用了这个默认的工作队列，则应该记住我们正在和他人共享该工作队列。
	这意味着，我们不应该长期独占该队列，即：不能长时间休眠，而且我们的任务可能需要更长的时间才能获得处理.
	工作队列一般用来做滞后的工作，比如在中断里面要做很多事，但是比较耗时，
	这时就可以把耗时的工作放到工作队列。说白了就是系统延时调度的一个自定义函数。
--------------------------------------------------------------------------
	和 tasklet 最大的不同是 工作队列的函数 可以使用休眠，而 tasklet 的函数是不允许使用休眠的。
工作队列的使用又分两种情况:

(一)利用系统共享的工作队列添加工作：
这种情况处理函数不能消耗太多时间，这样会影响共享队列中其他任务的处理;
	1、定义相关数据			  struct work_struct my_work;
	2、编写要提交到工作队列中的函数 (延时调度的一个自定义函数)
		static void my_func(struct work_struct *work) { … }
	3、完成数据的初始化工作		INIT_WORK(&my_work, my_func);
	4、将工作结构体变量添加到共享工作队列,工作完成后会自动从队列中删除
		schedule_work(&my_work);

(二)创建自己的工作队列来添加工作
　	1、+ 定义一个指向工作队列的指针 	struct workqueue_struct *p_queue;
		定义相关数据 			struct work_struct my_work;	
	2、编写要提交到工作队列中的函数 (延时调度的一个自定义函数)
		static void my_func(struct work_struct *work) { … }
	3、完成数据的初始化工作		INIT_WORK(&my_work, my_func);
	4、+ 创建自己的工作队列和工作结构体变量(通常在 open 函数中完成)
		p_queue=create_workqueue("my_queue"); 
		//创建一个名为 my_queue 的工作队列并把工作队列的入口地址赋给声明的指针
	5、将工作添加入自己创建的工作队列等待执行
		queue_work(p_queue, &my_work); //作用与 schedule_work() 类似，
		不同的是将工作添加入 p_queue 指针指向的工作队列而不是系统共享的工作队列
	6、删除自己的工作队列 (通常在 close / remove 函数中删除)
		destroy_workqueue(p_queue);
*/

/* 原子操作 只能对 整形变量 或者 位进行保护，
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
