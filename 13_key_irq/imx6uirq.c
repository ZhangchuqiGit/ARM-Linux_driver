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

/** 中断 开发板上的 KEY0 按键，不过我们采用中断的方式，并且采用定时器来实现按键消抖  */
//#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

/** 每个中断都有一个中断号，通过中断号即可区分不同的中断，有的资料也把中断号叫做中断线。
在 Linux 内核中使用一个 int 变量表示中断号。
--------------------------------------------------------------------------------
上半部与下半部
上半部：上半部 就是 中断处理函数。那些处理过程比较快、不会占用很长时间的代码就可以放在上半部完成。
下半部：如果中断处理过程比较耗时，那么就将这些比较耗时的代码提出来，
 	交给下半部去执行，这样 上半部（即中断处理函数）就会快进快出。
--------------------------------------------------------------------------------
 	Linux 内核将中断分为 上半部和下半部 的主要目的就是实现 中断处理函数(上半部)的快进快出，
那些对时间敏感、执行速度快的操作可以放到中断处理函数中，也就是上半部。
剩下的所有工作都可以放到下半部去执行，比如在上半部将数据拷贝到内存中，
关于数据的具体处理就可以放到下半部去执行。
	至于哪些代码属于上半部，哪些代码属于下半部并没有明确的规定，
一切根据实际使用情况去判断，这个就很考验驱动编写人员的功底了。
参考：
	①、如果要处理的内容不希望被其他中断打断，那么可以放到上半部。
 	②、如果要处理的任务对时间敏感，可以放到上半部。
	③、如果要处理的任务与硬件有关，可以放到上半部
	④、除了上述三点以外的其他任务，优先考虑放到下半部。
--------------------------------------------------------------------------------
	request_irq() 申请中断的时候注册的 中断服务函数(即上半部)，只要中断触发，
那么中断处理函数就会执行。我们都知道中断处理函数一定要快点执行完毕，越短越好，但是现实往往是残酷的，
有些中断处理过程是比较费时间，必须要对其进行处理，缩小中断处理函数的执行时间。
比如电容触摸屏通过中断通知 SOC 有触摸事件发生，SOC 响应中断，
然后通过 IIC 接口读取触摸坐标值并将其上报给系统。
但是我们都知道 IIC 的速度最高也只有 400Kbit/S，所以在中断中通过 IIC 读取数据就会浪费时间。
我们可以将通过 IIC 读取触摸数据的操作 暂后执行(下半部)，
中断处理函数仅仅相应中断，然后清除中断标志位即可。          **/

/* 上半部处理简单，编写中断处理函数就行，关键是下半部该怎么做呢？Linux 内核提供了多种 下半部 机制
--------------------------------------------------------------------------------
1、软中断（下半部）  (建议用 tasklet) 不可以睡眠
	Linux 内核使用结构体 softirq_action 表示软中断，
 softirq_action 结构体定义在文件 include/linux/interrupt.h 中。
 在 定义了 10 个软中断：static struct softirq_action softirq_vec[ NR_SOFTIRQS ];
 NR_SOFTIRQS 是枚举类型，定义在文件 include/linux/interrupt.h 中。
	softirq_action 结构体中的 action 成员变量就是软中断的服务函数，
数组 softirq_vec 是个全局数组，因此所有的 CPU(对于 SMP 系统而言)都可以访问到，
每个 CPU 都有自己的触发和控制机制，并且只执行自己所触发的软中断。
但是各个 CPU 所执行的软中断服务函数确是相同的，都是数组 softirq_vec 中定义的 action 函数。
--------------------------------
 注册软中断处理函数
void open_softirq(int nr, void (*action)(struct softirq_action *))
nr：要开启的软中断，在以下中选择一个。
	enum {
		HI_SOFTIRQ=0, 		高优先级软中断
		TIMER_SOFTIRQ, 		定时器软中断
		NET_TX_SOFTIRQ, 	网络数据发送软中断
		NET_RX_SOFTIRQ, 	网络数据接收软中断
		BLOCK_SOFTIRQ,
		BLOCK_IOPOLL_SOFTIRQ,
		TASKLET_SOFTIRQ, 	tasklet 软中断
		SCHED_SOFTIRQ, 		调度软中断
		HRTIMER_SOFTIRQ, 	高精度定时器软中断
		RCU_SOFTIRQ, 		RCU 软中断
		NR_SOFTIRQS
	};
action：软中断对应的处理函数。
--------------------------------
 软中断必须在编译的时候静态注册！Linux 内核使用 softirq_init 函数初始化软中断，
 softirq_init 函数定义在 kernel/softirq.c 文件里面。
 void __init softirq_init(void) {
 	...
 	open_softirq( TASKLET_SOFTIRQ, 	tasklet_action); 	// 默认用 tasklet
	open_softirq( HI_SOFTIRQ, 		tasklet_hi_action);	// 默认用 tasklet
 }
--------------------------------------------------------------------------------
2、tasklet（下半部）(建议使用) 不可以睡眠
	Linux 内核使用 tasklet_struct 结构体来表示 tasklet。
struct tasklet_struct
{
	struct tasklet_struct *next;	//下一个 tasklet
	unsigned long state;			//tasklet 状态
	atomic_t count;					//计数器，记录对 tasklet 的引用数
	void (*func)(unsigned long);	//tasklet 执行的函数（下半部）用户定义函数内容
	unsigned long data;				//函数 func 的参数
};
--------------------------------
初始化 tasklet
void tasklet_init(	struct tasklet_struct t,
					void (*func)(unsigned long),
					unsigned long data);
t：		要初始化的 tasklet
func： 	tasklet 的处理函数（下半部）
data： 	要传递给 func 函数的参数
-------
一次性完成 tasklet 的定义和初始化，可用 DECLARE_TASKLET 定义在 include/linux/interrupt.h
 DECLARE_TASKLET(name, func, data)
name 为要定义的 tasklet 名字，这个名字就是一个 tasklet_struct 类型的时候变量，
func 就是 tasklet 的处理函数， data 是传递给 func 函数的参数
--------------------------------
	在上半部(中断处理函数)中调用 tasklet_schedule 函数就能使 tasklet 在合适的时间运行
void tasklet_schedule(struct tasklet_struct *t) // 调度
t：要调度的 tasklet，也就是 DECLARE_TASKLET 宏里面的 name。
-------------------------------- tasklet 使用示例
struct tasklet_struct testtasklet; 				// 定义 taselet（下半部）
void testtasklet_func(unsigned long data) 		// tasklet 处理函数
{ tasklet 具体处理内容 }
irqreturn_t test_handler(int irq, void *dev_id) // 中断处理函数 (上半部)
{
	...
	tasklet_schedule(&testtasklet);	// 调度 tasklet
	...
}
static int __init xxxx_init(void) // 驱动入口函数
{
	...
	// 初始化 tasklet
	tasklet_init(&testtasklet, testtasklet_func, data);
	// 注册中断处理函数
	request_irq(xxx_irq, test_handler, 0, "xxx", &xxx_dev);
	...
}
--------------------------------------------------------------------------------
3、工作队列（下半部）可以睡眠
	工作队列在进程上下文执行，工作队列将要推后的工作交给一个内核线程去执行，
 工作队列工作在进程上下文，因此允许睡眠或重新调度。
 因此如果你要推后的工作可以睡眠那么就可以选择工作队列，否则的话就只能选择 软中断 或 tasklet。
--------------------------------
	Linux 内核使用 work_struct 结构体表示一个工作，工作队列使用 workqueue_struct 结构体表示，
用工作者线程(worker thread)来处理工作队列中的各个工作， 用 worker 结构体表示工作者线程。
	在实际的驱动开发中，我们只需要定义工作(work_struct)即可，
 关于工作队列和工作者线程我们基本不用去管。简单创建工作很简单，
 直接定义一个 work_struct 结构体变量即可，然后使用 INIT_WORK 宏来初始化工作。
 #define INIT_WORK(_work, _func) // 初始化工作
	_work 表示要初始化的工作， _func 是工作对应的处理函数（下半部）
--------
 	可以使用 DECLARE_WORK 宏一次性完成工作的创建和初始化，宏定义如下：
 #define DECLARE_WORK(n, f)
 	n 表示定义的工作(work_struct)， f 表示工作对应的处理函数。
--------------------------------
和 tasklet 一样，工作也是需要调度才能运行的
 bool schedule_work(struct work_struct *work) // 调度
 work： 要调度的工作		返回值： 0 成功，其他值 失败
-------------------------------- 工作队列 使用示例
struct work_struct testwork;	 				// 定义 工作
void testwork_func_t(unsigned long data) 		// work 处理函数（下半部）
{ work 具体处理内容 }
irqreturn_t test_handler(int irq, void *dev_id) // 中断处理函数 (上半部)
{
	...
	schedule_work(&testwork);	// 调度 work
	...
}
static int __init xxxx_init(void) // 驱动入口函数
{
	...
	// 初始化 work
	INIT_WORK(&testwork, testwork_func_t);
	// 注册中断处理函数
	request_irq(xxx_irq, test_handler, 0, "xxx", &xxx_dev);
	...
}          */

/* 在 Linux 内核中要想使用某个中断是需要申请的， request_irq 函数用于申请中断，注册中断处理函数，
request_irq 函数可能会导致睡眠，因此在 中断上下文 或者 其他禁止睡眠 的代码段中 不能使用。
request_irq 函数会激活(使能)中断，所以不需要我们手动去使能中断。
int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
				const char *name, void *dev)
irq		要申请中断的中断号
handler	中断处理函数，中断发生以后就会执行此函数 irqreturn_t (*handler)(int, void *);
 irqreturn_t (*handler)(int, void *); 中断处理函数 (上半部)
第一个参数 int 中断号。
第二个参数 指向 void 的指针，需要与 request_irq 函数的 dev 参数保持一致，
 		用于区分共享中断的不同设备，dev 也可以指向设备数据结构。
返回值
 IRQ_NONE			interrupt was not from this device
 IRQ_HANDLED	 	interrupt was handled by this device
 IRQ_WAKE_THREAD 	handler requests to wake the handler thread
------
flags	中断标志，可以在文件 include/linux/interrupt.h 里面查看所有的中断标志
	标志 				描述
	IRQF_SHARED 		多个设备共享一个中断线，共享的所有中断都必须指定此标志。
				如果使用共享中断的话， request_irq 函数的 dev 参数就是唯一区分他们的标志。
	IRQF_ONESHOT 		单次中断，中断执行一次就结束。
	IRQF_TRIGGER_NONE 	无触发。
	IRQF_TRIGGER_RISING 上升沿触发。
	IRQF_TRIGGER_FALLING 下降沿触发。
	IRQF_TRIGGER_HIGH 	高电平触发。
	IRQF_TRIGGER_LOW 	低电平触发。
name	中断名字，设置以后可以在 /proc/interrupts 文件中看到对应的中断名字。
dev 	如果将 flags 设置为 IRQF_SHARED 的话， dev 用来区分不同的中断，
 一般情况下将 dev 设置为设备结构体， dev 会传递给中断处理函数 irq_handler_t 的第二个参数。
返回值： 0 中断申请成功，其他负值 中断申请失败，返 -EBUSY 表示中断已被申请。
---------------------------------------
	通过 free_irq 函数释放掉相应的中断。
如果中断不是共享的，那么 free_irq 会删除中断处理函数并且禁止中断。
void free_irq(unsigned int irq, void *dev)
irq 	要释放的中断号
dev 	如果中断设置为共享(IRQF_SHARED)的话，此参数用来区分具体的中断。
共享中断只有在释放最后中断处理函数的时候才会被禁止掉。
---------------------------------------
使能或者禁止 某一个中断
irq		要申请中断的中断号
void enable_irq(unsigned int irq)
void disable_irq(unsigned int irq)
	disable_irq 函数要等到当前正在执行的中断处理函数执行完才返回，
 因此使用者需要保证不会产生新的中断，并且确保所有已经开始执行的中断处理程序已经全部退出！
 在这种情况下，可以使用另外一个中断禁止函数：
void disable_irq_nosync(unsigned int irq) 立即禁止中断，不会等待当前中断处理程序执行完毕。
---------------------------------------
使能或者禁止 当前处理器的整个中断系统/全局中断
local_irq_save(flags) 		禁止中断前，将中断状态保存在 flags 中
local_irq_restore(flags)	恢复中断到 flags 状态
local_irq_enable()			使能当前处理器中断系统
local_irq_disable()			禁止当前处理器中断系统
---------------------------------------
获取中断号
 	编写驱动的时候需要用到中断号，我们用到中断号，中断信息已经写到了设备树里面，
 因此可以通过 irq_of_parse_and_map 函数从 interupts 属性中提取到对应的设备号
 unsigned int irq_of_parse_and_map(struct device_node *dev, int index)
	dev： 	设备节点。
	index：	索引号，interrupts 属性可能包含多条中断信息，通过 index 指定要获取的信息。
	返回值：	中断号
----
	使用 GPIO 的话，可以使用 gpio_to_irq 函数来获取 gpio 对应的中断号
 int gpio_to_irq(unsigned int gpio)
	gpio： 	要获取的 GPIO 编号。
	返回值： GPIO 对应的中断号。    */

/** 一般 原子操作 用于 变量 或者 位操作 **/
#include <asm/atomic.h>
/** 一般 原子操作 用于 变量 或者 位操作 **/
/* 原子操作 API 函数 			描述
ATOMIC_INIT(int i) 				定义原子变量的时候对其初始化。
int atomic_read(atomic_t *v) 	读取 v 的值，并且返回。
void atomic_inc(atomic_t *v) 	给 v 加 1，也就是自增。
void atomic_dec(atomic_t *v) 	从 v 减 1，也就是自减
void atomic_set(atomic_t *v, int i) 	向 v 写入 i 值。
void atomic_add(int i, atomic_t *v) 	给 v 加上 i 值。
void atomic_sub(int i, atomic_t *v) 	从 v 减去 i 值。
int atomic_dec_return(atomic_t *v) 		从 v 减 1，并且返回 v 的值。
int atomic_inc_return(atomic_t *v) 		给 v 加 1，并且返回 v 的值。
int atomic_sub_and_test(int i, atomic_t *v) 从 v 减 i，如果结果为 0 就返回真，否则返回假
int atomic_dec_and_test(atomic_t *v) 		从 v 减 1，如果结果为 0 就返回真，否则返回假
int atomic_inc_and_test(atomic_t *v) 		给 v 加 1，如果结果为 0 就返回真，否则返回假
int atomic_add_negative(int i, atomic_t *v) 给 v 加 i，如果结果为负就返回真，否则返回假
------------------------------------------------------------------------------------
原子位操作 API 函数  					描述
void set_bit(int nr, void *p) 		将 p 地址的第 nr 位置 1。
void clear_bit(int nr,void *p) 		将 p 地址的第 nr 位清零。
void change_bit(int nr, void *p) 	将 p 地址的第 nr 位进行翻转。
int test_bit(int nr, void *p) 		获取 p 地址的第 nr 位的值。
int test_and_set_bit(int nr, void *p) 	将 p 地址的第 nr 位置 1，并且返回 nr 位原来的值。
int test_and_clear_bit(int nr, void *p) 将 p 地址的第 nr 位清零，并且返回 nr 位原来的值。
int test_and_change_bit(int nr, void *p) 将 p 地址的第 nr 位翻转，并且返回 nr 位原来的值*/

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

#define IMX6UIRQ_CNT        1            /* 设备号个数 	*/
#define IMX6UIRQ_NAME        "imx6uirq"    /* 名字 		*/

/* 定义按键值 */
#define KEY_0_VALUE            0X01        /* KEY0按键值 	*/
#define INVAKEY                0Xff        /* 无效的按键值 */

#define KEY_NUM                1            /* 按键数量 	*/

/* 寄存器物理地址：设备树方式不需要 */

/* 映射后的寄存器虚拟地址指针：princtl 方式不需要 */

/** 中断IO描述结构体 */
struct irq_keydesc {
	int gpio;                    // key 所使用的 Gled_switch PIO 编号
	int irqnum;                       /* 中断号     */
	unsigned char value;              /* 按键对应的键值 */
	char name[10];                    /* 名字 */
	irqreturn_t (*handler)(int, void *);    /* 中断服务函数 */
};

/* imx6uirq 设备结构体 */
struct imx6uirq_dev {
	dev_t devid;            /* 设备号 	 */
	struct cdev cdev;        /* cdev 	*/
	struct class *class;    /* 类 		*/
	struct device *device;    /* 设备 	 */
	int major;                /* 主设备号	  */
	int minor;                /* 次设备号   */
	struct device_node *nd; /* 设备节点 */

	atomic_t keyvalue;        /* 有效的按键键值 */
	atomic_t releasekey;    /* 标记是否完成一次完成的按键，包括按下和释放 */

	struct timer_list timer;/* 定义一个定时器*/
	unsigned char curkeynum;                /* 当前的按键号 */
	struct irq_keydesc irqkeydesc[KEY_NUM];    /* 按键描述数组 */
};

struct imx6uirq_dev imx6uirq;    /* irq设备 */

/* @description		: 中断服务函数，开启定时器，延时10ms，
 *				  	  定时器用于按键消抖。
 * @param - irq 	: 中断号 
 * @param - dev_id	: 设备结构。
 * @return 			: 中断执行结果
 */
static irqreturn_t key0_handler(int irq, void *dev_id) // 中断服务函数 (上半部)
{
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *) dev_id;
	dev->timer.data = (volatile long) dev_id;

	dev->curkeynum = 0; // 当前的按键号 KEY_0

	/* mod_timer 函数用于修改定时值，如果定时器还没有激活的话， mod_timer 函数会激活定时器 */
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));    /* 10ms定时 */
	return IRQ_RETVAL(IRQ_HANDLED);
}

/* @description	: 定时器服务函数/定时器回调函数，用于按键消抖，定时器到了以后
 *				  再次读取按键值，如果按键还是处于按下状态就表示按键有效。
 * @param - arg	: 设备结构变量
 * @return 		: 无
 */
void timer_function(unsigned long arg)
{
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *) arg;
	unsigned char num = dev->curkeynum; // 当前的按键号 KEY_0
	struct irq_keydesc *keydesc = &dev->irqkeydesc[num];
	unsigned char value = gpio_get_value(keydesc->gpio);    /* 读取IO值 */
	if (value == 0) {                        /* 按下按键 */
		atomic_set(&dev->keyvalue, keydesc->value);
	} else {                                    /* 按键松开 */
		atomic_set(&dev->keyvalue, INVAKEY);
		atomic_set(&dev->releasekey, 1);    /* 标记松开按键，即完成一次完整的按键过程 */
	}
}

/*
 * @description	: 初始化按键IO，open函数打开驱动的时候
 * 				  初始化按键所使用的GPIO引脚。
 * @param 		: 无
 * @return 		: 无
 */
static int keyio_init(void)
{
	unsigned char i = 0;
	int ret = 0;

	atomic_set(&imx6uirq.keyvalue, INVAKEY);
	atomic_set(&imx6uirq.releasekey, 0);

	/* 获取设备树中的属性数据 设置所使用的GPIO */
	/* 1、获取设备节点 */
	imx6uirq.nd = of_find_node_by_path("/key");
	if (imx6uirq.nd == NULL) {
		printk("key node not find!\r\n");
		return -EINVAL;
	}

	/* 2、 获取设备树中的gpio属性，得到 key 所使用的编号 */
	for (i = 0; i < KEY_NUM; i++) {
		imx6uirq.irqkeydesc[i].gpio = of_get_named_gpio(imx6uirq.nd, "key-gpio", i);
		if (imx6uirq.irqkeydesc[i].gpio < 0) {
			printk("can't get key%d\r\n", i);
		} else printk("key_gpio num = %d\r\n", imx6uirq.irqkeydesc->gpio);
	}

	/* 初始化 key 所使用的 IO，并且设置成中断模式 */
	for (i = 0; i < KEY_NUM; i++) {
		memset(imx6uirq.irqkeydesc[i].name, 0, sizeof(imx6uirq.irqkeydesc[i].name));    /* 缓冲区清零 */
		sprintf(imx6uirq.irqkeydesc[i].name, "KEY%d", i);        /* 组合名字 */

		/* 初始化 key 所使用的 IO */
		gpio_request(imx6uirq.irqkeydesc[i].gpio, imx6uirq.irqkeydesc[i].name);/* 请求IO */

		/* 3、设置为输入 */
		gpio_direction_input(imx6uirq.irqkeydesc[i].gpio);

		imx6uirq.irqkeydesc[i].irqnum = (int) irq_of_parse_and_map(imx6uirq.nd, i);
		// 获取设备树对应的中断号
#if 0
		imx6uirq.irqkeydesc[i].irqnum = gpio_to_irq(imx6uirq.irqkeydesc[i].gpio);
		// 获取 gpio 对应的中断号，设置中断触发模式
#endif
		printk("key%d:gpio=%d, irqnum=%d\r\n", i, imx6uirq.irqkeydesc[i].gpio,
			   imx6uirq.irqkeydesc[i].irqnum);
	}

	/* 申请中断 */
	imx6uirq.irqkeydesc[0].handler = key0_handler; // 中断服务函数 (上半部)
	imx6uirq.irqkeydesc[0].value = KEY_0_VALUE;

	/* 轮流调用 request_irq 函数申请中断号，
	设置中断触发模式为 IRQF_TRIGGER_FALLING 和 IRQF_TRIGGER_RISING，
	也就是 上升沿 和 下降沿 都可以触发中断 */
	for (i = 0; i < KEY_NUM; i++) {
		/* 在 Linux 内核中要想使用某个中断是需要申请的， request_irq 函数用于申请中断，
		注册中断处理函数，request_irq 函数可能会导致睡眠，
		因此在 中断上下文 或者 其他禁止睡眠 的代码段中 不能使用。
		request_irq 函数会激活(使能)中断，所以不需要我们手动去使能中断。 */
		/** 注册中断处理函数，使能中断 */
		ret = request_irq(imx6uirq.irqkeydesc[i].irqnum, imx6uirq.irqkeydesc[i].handler,
						  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, imx6uirq.irqkeydesc[i].name,
						  &imx6uirq);
		if (ret < 0) {
			printk("irq %d request failed!\r\n", imx6uirq.irqkeydesc[i].irqnum);
			return -EFAULT;
		}
	}

	/** 创建定时器 */
	init_timer(&imx6uirq.timer); // 初始化定时器
	imx6uirq.timer.function = timer_function; // 设置定时器的定时处理函数
	return 0;
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int imx6uirq_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &imx6uirq;    /* 设置私有数据 */
	return 0;
}

/*
 * @description     : 从设备读取数据
 * @param - filp    : 要打开的设备文件(文件描述符)
 * @param - buf     : 返回给用户空间的数据缓冲区
 * @param - cnt     : 要读取的数据长度
 * @param - offt    : 相对于文件首地址的偏移
 * @return          : 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t imx6uirq_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int ret = 0;
	unsigned char keyvalue = 0;
	unsigned char releasekey = 0;
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *) filp->private_data;    /* 获取私有数据 */

	keyvalue = atomic_read(&dev->keyvalue);
	releasekey = atomic_read(&dev->releasekey); // 按下标志
	if (releasekey) { /* 有按键按下 */
		if (keyvalue == INVAKEY) {
			keyvalue = dev->irqkeydesc[dev->curkeynum].value;//按键对应的键值

			/* 	copy_to_user() 将数据从内核空间拷贝到用户空间,一般是在驱动中 chr_drv_read()用
		参数1：内核驱动中的一个buffer
		参数2：应用空间到一个buffer
		参数3：个数   */
			ret = copy_to_user(buf, &keyvalue, sizeof(keyvalue));
		} else {
			goto data_error;
		}
		atomic_set(&dev->releasekey, 0);    // 按下标志清零
		return 1;
	}
	ret = copy_to_user(buf, &keyvalue, sizeof(keyvalue));
	return 0;

data_error:
	return -EINVAL;
}

/* 设备操作函数 */
static struct file_operations imx6uirq_fops = {
		.owner = THIS_MODULE,
		.open = imx6uirq_open,
		.read = imx6uirq_read
};

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init

imx6uirq_init(void)
{
	/* 注册字符设备驱动 */
	/* 1、构建设备号 */
	if (imx6uirq.major) {/*  定义了设备号 */
		imx6uirq.devid = MKDEV(imx6uirq.major, 0);
		register_chrdev_region(imx6uirq.devid, IMX6UIRQ_CNT, IMX6UIRQ_NAME);
	} else {/* 没有定义设备号 */
		alloc_chrdev_region(&imx6uirq.devid, 0, IMX6UIRQ_CNT, IMX6UIRQ_NAME);/* 申请设备号 */
		imx6uirq.major = MAJOR(imx6uirq.devid);/* 获取分配号的主设备号 */
		imx6uirq.minor = MINOR(imx6uirq.devid);/* 获取分配号的次设备号 */
	}

	/* 2、初始化cdev */
	cdev_init(&imx6uirq.cdev, &imx6uirq_fops);

	/* 3、添加一个cdev */
	cdev_add(&imx6uirq.cdev, imx6uirq.devid, IMX6UIRQ_CNT);

	/* 3、创建类 */
	imx6uirq.class = class_create(THIS_MODULE, IMX6UIRQ_NAME);
	if (IS_ERR(imx6uirq.class)) {
		return PTR_ERR(imx6uirq.class);
	}

	/* 5、创建设备 */
	imx6uirq.device = device_create(imx6uirq.class, NULL, imx6uirq.devid, NULL, IMX6UIRQ_NAME);
	if (IS_ERR(imx6uirq.device)) {
		return PTR_ERR(imx6uirq.device);
	}

	/** 6、初始化按键 */
	keyio_init();
	return 0;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit imx6uirq_exit(void)
{
	unsigned int i = 0;
	/* 删除定时器 */
	del_timer_sync(&imx6uirq.timer);    /* 删除定时器 */

	/* 释放中断 */
	for (i = 0; i < KEY_NUM; i++) {
		free_irq(imx6uirq.irqkeydesc[i].irqnum, &imx6uirq);
	}
	cdev_del(&imx6uirq.cdev);
	unregister_chrdev_region(imx6uirq.devid, IMX6UIRQ_CNT);
	device_destroy(imx6uirq.class, imx6uirq.devid);
	class_destroy(imx6uirq.class);
}

module_init(imx6uirq_init);
module_exit(imx6uirq_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
