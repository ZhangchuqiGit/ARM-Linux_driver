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
#include <linux/of_irq.h>
#include <linux/irq.h>

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

#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/** 阻塞和非阻塞 IO 是 Linux 驱动开发里面很常见的两种设备访问模式，
 * 在编写驱动的时候一定要考虑到阻塞和非阻塞。
 * 阻塞访问最大的好处就是当设备文件不可操作的时候进程可以进入休眠态，这样可以将CPU 资源让出来。
 * 但是，当设备文件可以操作的时候就必须唤醒进程，一般在中断函数里面完成唤醒工作。 */

/* 	等待队列(wait queue)
 	Linux 内核提供了等待队列(wait queue)来实现阻塞进程的唤醒工作，如果我们要在驱动中使用等待队列，
必须创建并初始化一个等待队列头，等待队列头使用结构体 wait_queue_head_t 表示，
wait_queue_head_t 结构体定义在文件 include/linux/wait.h 中
---------------------
初始化 等待队列头 (就是一个等待队列的头部)
void init_waitqueue_head(wait_queue_head_t *q)
参数 q 就是要初始化的等待队列头。
也可以使用宏 DECLARE_WAIT_QUEUE_HEAD 来一次性完成等待队列头的定义的初始化。
---------------------
等待队列项（每个访问设备）
	每个访问设备的进程都是一个队列项，当设备不可用的时候就要将这些进程对应的等待队列项
添加到等待队列里面。结构体 wait_queue_t 表示等待队列项。
	使用宏 DECLARE_WAITQUEUE(name, tsk) 定义并初始化一个等待队列项。
name 就是等待队列项的名字， tsk 表示这个等待队列项属于哪个任务(进程)，一般设置为 current，
在 Linux 内核中 current 相当于一个全局变量，表示当前进程。
因此宏 DECLARE_WAITQUEUE 就是给当前正在运行的进程创建并初始化了一个等待队列项。
---------------------
	当设备不可访问的时候就需要将进程对应的 等待队列项 添加到前面创建的 等待队列头 中，
只有添加到等待队列头中以后进程才能进入休眠态。
当设备可以访问以后再将进程对应的 等待队列项 从 等待队列头 中移除即可。
队列项 添加 等待队列头
 void add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
	q：等待队列项要加入的等待队列头。			wait：要加入的等待队列项。
队列项 移除 等待队列头
void remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
	q：要删除的等待队列项所处的等待队列头。		wait：要删除的等待队列项。
---------------------
唤醒当前进程		当设备可以使用的时候就要唤醒进入休眠态的进程。
void wake_up(wait_queue_head_t *q)
void wake_up_interruptible(wait_queue_head_t *q)
	参数 q 就是要唤醒的等待队列头，这两个函数会将这个等待队列头中的所有进程都唤醒。
	wake_up 函数可以唤醒 处于 TASK_INTERRUPTIBLE 和 TASK_UNINTERRUPTIBLE 状态的进程，
 而 wake_up_interruptible 函数只能唤醒处于 TASK_INTERRUPTIBLE 状态的进程。
---------------------
等待事件
	除了主动唤醒以外，也可以设置等待队列等待某个事件，当这个事件满足以后就自动唤醒等待队列中的进程。
wait_event(wq, condition)
 			等待以 wq 为等待队列头的等待队列被唤醒，
 			前提是 condition 条件必须满足(为真)，否则一直阻塞。
 			此函数会将进程设置为 TASK_UNINTERRUPTIBLE 状态。
 			此函数会将进程设置为 TASK_UNINTERRUPTIBLE 状态。
wait_event_timeout(wq, condition, timeout)
 			功能和 wait_event 类似，但是此函数可以添加超时时间，以 jiffies 为单位。
 			此函数有返回值，如果返回 0 的话表示超时时间到，而且 condition 为假。
 			为 1 的话表示 condition 为真，也就是条件满足了。
wait_event_interruptible(wq, condition)
 			与 wait_event 函数类似，此函数也将进程设置为 TASK_INTERRUPTIBLE，可以被信号打断。
wait_event_interruptible_timeout(wq, condition, timeout)
 			与 wait_event_timeout 函数类似，
 			此函数也将进程设置为 TASK_INTERRUPTIBLE，可以被信号打断。
--------------------------------------------------------------------------------
轮询
	如果用户应用程序以非阻塞的方式访问设备，设备驱动程序就要提供非阻塞的处理方式，也就是轮询。
 	poll、 epoll 和 select 可以用于处理轮询，
应用程序通过 select、 epoll 或 poll 函数来查询设备是否可以操作，
如果可以操作的话就从设备读取或者向设备写入数据。
当应用程序调用 select、 epoll 或 poll 函数的时候，设备驱动程序中的 poll 函数就会执行，
因此需要在设备驱动程序中编写 poll 函数。
	传统的 selcet 和 poll 函数都会随着所监听的 fd 数量的增加，出现效率低下的问题，
而且 poll 函数每次必须遍历所有的描述符来检查就绪的描述符，这个过程很浪费时间。
为此，epoll 应运而生， epoll 就是为处理大并发而准备的，一般常常在网络编程中使用 epoll 函数。 */

/* 	Linux 驱动下的 poll 操作函数
	当应用程序调用 select 或 poll 函数来对驱动程序进行非阻塞访问的时候，
驱动程序 file_operations（设备操作函数） 操作集中的 poll 函数就会执行。
所以驱动程序的编写者需要提供对应的 poll 函数。
unsigned int (*poll) (struct file *filp, struct poll_table_struct *wait)
filp： 要打开的设备文件(文件描述符)。
wait： 由应用程序传递进来的。一般将此参数传递给 poll_wait 函数。
返回值:	向应用程序返回设备或者资源状态，可以返回的资源状态如下：
	监听事件	返回状态			合法状态
	events	revents			POLLIN:        有普通数据或者优先数据可读
	events	revents			POLLRDNORM:    有普通数据可读
	events	revents			POLLRDBAND:    有优先数据可读
	events	revents			POLLPRI:       有紧急数据可读

	events	revents			POLLOUT:       有普通数据可写
	events	revents			POLLWRNORM:    有普通数据可写
	events	revents			POLLWRBAND:    有紧急数据可写

			revents			POLLERR:       有 错误 发生
			revents			POLLHUP:       有描述符 挂起 事件发生
			revents			POLLNVAL:      描述符 非法
POLLIN | POLLPRI 等价与 select() 的可读事件；
POLLOUT | POLLWRBAND 等价与 select() 的可写事件；
POLLIN 等价与 POLLRDNORM | POLLRDBAND，而 POLLOUT 等价于 POLLWRBAND。
如果你对一个描述符的可读事件和可写事件以及错误等事件均感兴趣那么你应该都进行相应的设置。
---------------------------------------------------------------------------
	我们需要在驱动程序的 poll 函数中调用 poll_wait 函数， poll_wait 函数不会引起阻塞，
只是将应用程序添加到 poll_table 中。
void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
	参数 wait_address 是要添加到 poll_table 中的等待队列头，
 	参数 p 就是 file_operations（设备操作函数） 中 poll 函数的 wait 参数。   */

#include <linux/fcntl.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h> // 异步通知

/* 异步通知 需要在 设备驱动 中实现 file_operations 操作集中的 fasync 函数
int (*fasync) (int fd, struct file *filp, int on)
--------------------------------
fasync 函数里面一般通过调用 fasync_helper 函数来初始化前面定义的 fasync_struct 结构体
int fasync_helper(int fd, struct file * filp, int on, struct fasync_struct **fapp)
 	fasync_helper 函数的前三个参数就是 fasync 函数的那三个参数，
 第四个参数就是要初始化的 fasync_struct 结构体指针变量。
 当应用程序通过 “fcntl(fd, F_SETFL, flags | FASYNC)” 改变 fasync 标记的时候，
 设备操作函数 file_operations 操作集中的 fasync 函数就会执行。
-------------------------------- 参考示例
struct xxx_dev { // 设备结构体
	...
	struct fasync_struct *async_queue; // 异步相关结构体
};
static int xxx_fasync(int fd, struct file *filp, int on) {
	struct xxx_dev *dev = (xxx_dev)filp->private_data; // 设置私有数据
	if (fasync_helper(fd, filp, on, &dev->async_queue) < 0) return -EIO;
	return 0;
}
static int xxx_release(struct inode *inode, struct file *filp) {
	return xxx_fasync(-1, filp, 0); // 删除异步通知
}
static struct file_operations xxx_ops = { // 设备操作函数
	...
	.fasync = xxx_fasync,
	.release = xxx_release,
	...
};
--------------------------------
	当设备可以访问的时候，驱动程序需要向应用程序发出信号，相当于产生“中断”。
 kill_fasync 函数负责发送指定的信号
 void kill_fasync(struct fasync_struct **fp, int sig, int band)
	fp：	要操作的 fasync_struct。
	sig： 	要发送的信号。
	band： 	可读时设置为 POLL_IN，可写时设置为 POLL_OUT。
--------------------------------
应用程序 对 异步通知 的处理包括以下 三步：
	1、注册信号处理函数
			应用程序根据驱动程序所使用的信号来设置信号的处理函数，
		应用程序使用 signal 函数来设置信号的处理函数。
	2、将本应用程序的进程号告诉给内核
		fcntl(fd, F_SETOWN, getpid()); // 设置当前进程接收SIGIO信号
	3、开启异步通知
			flags = fcntl(fd, F_GETFL); 		// 获取当前的进程状态
			fcntl(fd, F_SETFL, flags | FASYNC);	// 开启当前进程异步通知功能
		重点就是通过 fcntl 函数设置进程状态为 FASYNC，经过这一步，
		驱动程序中的 fasync 函数就会执行  */

#define IMX6UIRQ_CNT        1            /* 设备号个数 	*/
#define IMX6UIRQ_NAME        "asyncnoti"    /* 名字 		*/

#define KEY0VALUE            0X01        /* KEY0按键值 	*/
#define INVAKEY                0XFF        /* 无效的按键值 */

#define KEY_NUM                1            /* 按键数量 	*/

/* 寄存器物理地址：设备树方式不需要 */

/* 映射后的寄存器虚拟地址指针：princtl 方式不需要 */

/** 中断IO描述结构体 */
struct irq_keydesc {
	int gpio;                         // key 所使用的 Gled_switch PIO 编号
	int irqnum;                                /* 中断号     */
	unsigned char value;                    /* 按键对应的键值 */
	char name[10];                            /* 名字 */
	irqreturn_t (*handler)(int, void *);    /* 中断服务函数 */
};

/* imx6uirq设备结构体 */
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
	struct irq_keydesc irqkeydesc[KEY_NUM];    /* 按键init述数组 */
	unsigned char curkeynum;                /* 当前init按键号 */

	wait_queue_head_t r_wait;    /* 读等待队列头
	阻塞访问最大的好处就是当设备文件不可操作的时候进程可以进入休眠态，这样可以将 CPU 资源让出来。
 但是，当设备文件可以操作的时候就必须唤醒进程，一般在中断函数里面完成唤醒工作。
 Linux 内核提供了等待队列(wait queue)来实现阻塞进程的唤醒工作  */
	struct fasync_struct *async_queue;        /* 异步相关结构体 */
};

struct imx6uirq_dev imx6uirq;    /* irq设备 */

/* @description		: 中断服务函数，开启定时器		
 *				  	  定时器用于按键消抖。
 * @param - irq 	: 中断号 
 * @param - dev_id	: 设备结构。
 * @return 			: 中断执行结果
 */
static irqreturn_t key0_handler(int irq, void *dev_id)
{
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *) dev_id;
	dev->timer.data = (volatile long) dev_id;

	dev->curkeynum = 0; // 当前的按键号 KEY_0

	/* mod_timer 函数用于修改定时值，如果定时器还没有激活的话， mod_timer 函数会激活定时器 */
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

	/** 发送 SIGIO 信号“中断” */
	if (atomic_read(&dev->releasekey)) {        /* 完成一次按键过程 */
		if (dev->async_queue)
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);    /* 释放 SIGIO 信号
			当设备可以访问的时候，驱动程序需要向应用程序发出信号，相当于产生“中断”。 
			kill_fasync 函数负责发送指定的信号 */
	}

#if 0 /** 加入等待队列，等待被唤醒,也就是有按键按下 */
	/** 唤醒进程 */
	if(atomic_read(&dev->releasekey)) {	/* 完成一次按键过程 */
		/* wake_up(&dev->r_wait); */
		wake_up_interruptible(&dev->r_wait); // 唤醒当前进程
	}
#endif
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
		}
	}

	/* 初始化key所使用的IO，并且设置成中断模式 */
	for (i = 0; i < KEY_NUM; i++) {
		memset(imx6uirq.irqkeydesc[i].name, 0, sizeof(name));    /* 缓冲区清零 */
		sprintf(imx6uirq.irqkeydesc[i].name, "KEY%d", i);        /* 组合名字 */

		/* 初始化 key 所使用的 IO */
		gpio_request(imx6uirq.irqkeydesc[i].gpio, name);

		/* 3、设置为输入 */
		gpio_direction_input(imx6uirq.irqkeydesc[i].gpio);

		imx6uirq.irqkeydesc[i].irqnum = (int) irq_of_parse_and_map(imx6uirq.nd, i);
		// 获取设备树对应的中断号
#if 0
imx6uirq.irqkeydesc[i].irqnum = gpio_to_irq(imx6uirq.irqkeydesc[i].gpio);
// 获取 gpio 对应的中断号，设置中断触发模式
#endif
	}

	/* 申请中断 */
	imx6uirq.irqkeydesc[0].handler = key0_handler; // 中断服务函数 (上半部)
	imx6uirq.irqkeydesc[0].value = KEY0VALUE;

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

	/** 初始化等待队列头 */
	init_waitqueue_head(&imx6uirq.r_wait);
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
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *) filp->private_data;/* 获取私有数据 */

	if (filp->f_flags & O_NONBLOCK) { /* 非阻塞访问 */
		if (atomic_read(&dev->releasekey) == 0)    /* 没有按键按下，返回-EAGAIN */
			return -EAGAIN;
	} else {                            /* 阻塞访问 */
		/** 加入等待队列，等待被唤醒,也就是有按键按下 */
		ret = wait_event_interruptible(dev->r_wait, atomic_read(&dev->releasekey));
		if (ret) {
			return ret;
		}
	}

	keyvalue = atomic_read(&dev->keyvalue);
	releasekey = atomic_read(&dev->releasekey);
	if (releasekey) { /* 有按键按下 */
		if (keyvalue == INVAKEY) {
			keyvalue = dev->irqkeydesc[dev->curkeynum].value; //按键对应的键值

			/* 	copy_to_user() 将数据从内核空间拷贝到用户空间,一般是在驱动中 chr_drv_read()用
参数1：内核驱动中的一个buffer
参数2：应用空间到一个buffer
参数3：个数   */
			ret = copy_to_user(buf, &keyvalue, sizeof(keyvalue));
		} else {
			goto data_error;
		}
		atomic_set(&dev->releasekey, 0); // 按下标志清零
	} else {
		goto data_error;
	}
	return ret;

data_error:
	return -EINVAL;
}

/*
 * @description     : poll函数，用于处理非阻塞访问
 * @param - filp    : 要打开的设备文件(文件描述符)
 * @param - wait    : 等待列表(poll_table)
 * @return          : 设备或者资源状态，
 */
unsigned int imx6uirq_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *) filp->private_data; /* 获取私有数据 */

	/**	我们需要在驱动程序的 poll 函数中调用 poll_wait 函数， poll_wait 函数不会引起阻塞，
只是将应用程序添加到 poll_table 中。
void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
	参数 wait_address 是要添加到 poll_table 中的等待队列头，
 	参数 p 就是 file_operations（设备操作函数） 中 poll 函数的 wait 参数。   */
	poll_wait(filp, &dev->r_wait, wait);    /* 将等待队列头添加到 poll_table 中 */

	if (atomic_read(&dev->releasekey)) {        /* 按键按下 */
		mask = POLLIN | POLLRDNORM;            /* 返回PLLIN */
	}
	return mask;
}

/**
 * @description     : fasync函数，用于处理异步通知
 * @param - fd		: 文件描述符
 * @param - filp    : 要打开的设备文件(文件描述符)
 * @param - on      : 模式
 * @return          : 负数表示函数执行失败
 */
static int imx6uirq_fasync(int fd, struct file *filp, int on)
{
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *) filp->private_data;  /* 获取私有数据 */
	return fasync_helper(fd, filp, on, &dev->async_queue); // 初始化
}

/*
 * @description     : release函数，应用程序调用close关闭驱动文件的时候会执行
 * @param - inode	: inode节点
 * @param - filp    : 要打开的设备文件(文件描述符)
 * @return          : 负数表示函数执行失败
 */
static int imx6uirq_release(struct inode *inode, struct file *filp)
{
	return imx6uirq_fasync(-1, filp, 0);
}

/* 设备操作函数 */
static struct file_operations imx6uirq_fops = {
		.owner = THIS_MODULE,
		.open = imx6uirq_open,
		.read = imx6uirq_read,
		.poll = imx6uirq_poll,
		.fasync = imx6uirq_fasync,
		.release = imx6uirq_release,
};

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init

imx6uirq_init(void)
{
	/* 1、构建设备号 */
	if (imx6uirq.major) {
		imx6uirq.devid = MKDEV(imx6uirq.major, 0);
		register_chrdev_region(imx6uirq.devid, IMX6UIRQ_CNT, IMX6UIRQ_NAME);
	} else {
		alloc_chrdev_region(&imx6uirq.devid, 0, IMX6UIRQ_CNT, IMX6UIRQ_NAME);
		imx6uirq.major = MAJOR(imx6uirq.devid);
		imx6uirq.minor = MINOR(imx6uirq.devid);
	}

	/* 2、注册字符设备 */
	cdev_init(&imx6uirq.cdev, &imx6uirq_fops);

	/* 3、添加一个cdev */
	cdev_add(&imx6uirq.cdev, imx6uirq.devid, IMX6UIRQ_CNT);

	/* 4、创建类 */
	imx6uirq.class = class_create(THIS_MODULE, IMX6UIRQ_NAME);
	if (IS_ERR(imx6uirq.class)) {
		return PTR_ERR(imx6uirq.class);
	}

	/* 5、创建设备 */
	imx6uirq.device = device_create(imx6uirq.class, NULL, imx6uirq.devid, NULL, IMX6UIRQ_NAME);
	if (IS_ERR(imx6uirq.device)) {
		return PTR_ERR(imx6uirq.device);
	}

	/** 6、始化按键 */
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
	unsigned i = 0;
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

/**********************	信号 signal **********************/
/*	信号机制是进程之间相互传递消息的一种方法，信号全称为软中断信号。
从它的命名可以看出，它的实质和使用很象中断。所以，信号可以说是进程控制的一部分。
	软中断信号（signal，又简称为信号）用来通知进程发生了异步事件。
进程之间可以互相通过系统调用 kill 发送软中断信号。内核也可以因为内部事件而给进程发送信号，
通知进程发生了某个事件。注意，信号只是用来通知某进程发生了什么事件，并不给该进程传递任何数据。
---------------------------------------------------------------------------
进程通过系统调用 signal 来指定进程对某个信号的处理行为。
在进程表的表项中有一个软中断信号域，该域中每一位对应一个信号，
当有信号发送给进程时，对应位置位。由此可以看出，进程对不同的信号可以同时保留，
但对于同一个信号，进程并不知道在处理之前来过多少个。
---------------------------------------------------------------------------
	收到信号的进程对各种信号有不同的处理方法。
处理方法可以分为三类：
第一种方法是，自定义，类似中断的处理程序，对于需要处理的信号，
 	进程可以指定处理函数，由该函数来处理。
第二种方法是，忽略 接收到信号后不做任何反应。
 	忽略某个信号，对该信号不做任何处理，就象未发生过一样。
第三种方法是，默认 接收到信号后按默认的行为处理该信号。 这是多数应用采取的处理方式，
 	对该信号的处理保留系统的默认值，这种缺省操作，对大部分的信号的缺省操作是使得进程终止。	 */

/**********************	信号的来源 **********************/
/*
1 信号来自内核， 生成信号的请求来自以下3个地方。
（1）用户
 	用户可以通过输入Ctrl-C, Ctrl-\等命令，
 	或是终端驱动程序分配给信号控制字符的其他任何键来请求内核产生信号。
（2）内核
	当进程执行出错时， 内核给进程发送一个信号。
 	例如，非法段存取，浮点数溢出，亦或是一个非法指令，内核也利用信号通知进程特定事件发生。
（3）进程
	一个进程可以通过系统调用kill给另外一个进程发送信号， 一个进程可以和另一个进程通过信号通信。
2 信号捕获处理，进程能够通过系统调用 signal 告诉内核， 它要如何处理信号， 进程有3个选择。
（1）接收默认处理（通常是消亡）
 	SIGINT 的默认处理是消亡， 进程并不一定要使用signal接收默认处理，
 	但是进程能够通过以下调用来恢复默认处理。signal(SIGINT, SIG_DFL);
（2）忽略信号
	程序可以通过以下调用来告诉内核， 它需要忽略 SIGINT。signal(SIGINT, SIG_IGN);
（3）信号处理函数
	程序能够告诉内核，当程序到来时应该调用哪个函数。 signal(signum, functionname); */

/**********************	Linux 信号 相关函数 **********************/
/*	信号安装
		进程处理某个信号前，需要先在进程中安装此信号。
		安装过程主要是建立 信号值 和 进程对相应信息值的 动作。
int signal(int signum, sighandler_t handler); // 不支持信号传递信息，主要用于 非实时信号
int sigaction(int signum, struct sigaction *act, sigaction *oact);
	// 支持信号传递信息，主要用于实时信号，可用于所有信号(含非实时信号)
---------------------------------------------------------------------------
	信号发送
int kill(pid_t pid, int signum); // 用于向进程或进程组发送信号
int sigqueue (pid_t pid, int signum, const union sigval val);
	// 只能向 一个 进程发送信号，不能向进程组发送信号；
 	// 主要针对实时信号提出，与 sigaction() 组合使用，支持非实时信号的发送；
unsigned int alarm (unsigned int seconds); // 计时达到后给进程发送 SIGALARM 信号
int setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value);		//	getitimer(which, curr_value);
 	// 设置定时器，计时达到后给进程发送 SIGALRM 信号，功能比 alarm()强大 <sys/time.h>
void abort(void); // 向进程发送 中止执行 SIGABORT 信号，默认进程会异常退出。
int raise(int signum); // 向进程自身发送信号
---------------------------------------------------------------------------
信号集操作函数
sigemptyset(sigset_t *set)：信号集全部清0；
sigfillset(sigset_t *set)： 信号集全部置1，则信号集包含linux支持的64种信号；
sigaddset(sigset_t *set, int signum)：向信号集中加入signum信号；
sigdelset(sigset_t *set, int signum)：向信号集中删除signum信号；
sigismember(const sigset_t *set, int signum)：判定信号signum是否存在信号集中。
---------------------------------------------------------------------------
信号阻塞(屏蔽)函数
sigprocmask(int how, const sigset_t *set, sigset_t *oldset));//不同how参数，实现不同功能
SIG_BLOCK：将set指向信号集中的信号，添加到进程阻塞信号集；
SIG_UNBLOCK：将set指向信号集中的信号，从进程阻塞信号集删除；
SIG_SETMASK：将set指向信号集中的信号，设置成进程阻塞信号集；
sigpending(sigset_t *set))：获取已发送到进程，却被阻塞的所有信号；
sigsuspend(const sigset_t *mask))：用mask代替进程的原有掩码，并暂停进程执行，
 	直到收到信号再恢复原有掩码并继续执行进程。			*/

/**********************	Linux 支持的信号列表 **********************/
/*	值 的含义：
		第一个值 通常在 Alpha 和 Sparc 上有效，
		中间值 	对应 i386 和 ppc 以及 sh，
		最后值 	对应 mips。
		- 表示信号没有实现
---------------------------------------------------------------------------
 	处理动作 的字母含义：
					A 缺省的动作是 终止进程
					B 缺省的动作是 忽略此信号
					C 缺省的动作是 终止进程 并 进行内核映像转储（dump core）
					D 缺省的动作是 停止进程
					E 信号 不能 被捕获
					F 信号 不能 被忽略
---------------------------------------------------------------------------
	POSIX.1 中列出的信号
信号	值    处理动作	发出信号的原因
SIGHUP 	1 		 	A 	终端挂起或者控制进程终止
SIGINT	2 			A 	键盘中断（如break键被按下）
SIGQUIT 3 	 		C 	键盘的退出键被按下
SIGILL 	4	 	 	C 	非法指令
SIGABRT 6 		 	C 	由abort(3)发出的退出指令
SIGFPE 	8 		 	C 	浮点异常
SIGKILL 9 		 	AEF Kill信号
SIGSEGV 11 		 	C 	无效的内存引用
SIGPIPE 13 		 	A 	管道破裂: 写一个没有读端口的管道
SIGALRM 14 		 	A 	由alarm(2)发出的信号
SIGTERM 15 		 	A 	终止信号
SIGUSR1 30,10,16 	A 	用户自定义信号1
SIGUSR2 31,12,17 	A 	用户自定义信号2
SIGCHLD 20,17,18	B 	子进程结束信号
SIGCONT 19,18,25 		进程继续（曾被停止的进程）
SIGSTOP 17,19,23 	DEF 终止进程
SIGTSTP 18,20,24 	D 	控制终端（tty）上按下停止键
SIGTTIN 21,21,26 	D 	后台进程企图从控制终端读
SIGTTOU 22,22,27 	D 	后台进程企图从控制终端写
---------------------------------------------------------------------------
	没在 POSIX.1 中列出，而在 SUSv2 列出的信号
信号	值    		处理动作	发出信号的原因
SIGBUS 	10,7,10 	C 		总线错误(错误的内存访问)
SIGPOLL 			A 		Sys V定义的 Pollable 事件，与 SIGIO 同义
SIGPROF	27,27,29 	A 		Profiling定时器到
SIGSYS 	12,-,12 	C 		无效的系统调用 (SVID)
SIGTRAP 5 			C 		跟踪/断点捕获
SIGURG 	16,23,21 	B 		Socket出现紧急条件(4.2 BSD)
SIGVTALRM 26,26,28 	A 		实际时间报警时钟信号(4.2 BSD)
SIGXCPU 24,24,30 	C 		超出设定的CPU时间限制(4.2 BSD)
SIGXFSZ 25,25,31 	C 		超出设定的文件大小限制(4.2 BSD)
对于 SIGBUS，SIGSYS，SIGXCPU，SIGXFSZ，Linux 缺省的动作是 A ，SUSv2 是 C
---------------------------------------------------------------------------
	其它的信号
信号		值    		处理动作	发出信号的原因
SIGIOT 		6 			C 		IO捕获指令，与SIGABRT同义
SIGEMT 		7,-,7
SIGSTKFLT 	-,16,- 		A 		协处理器堆栈错误
SIGIO 		23,29,22 	A 		某I/O操作现在可以进行了(4.2 BSD)
SIGCLD 		-,-,18 		A 		与SIGCHLD同义
SIGPWR 		29,30,19 	A 		电源故障(System V)
SIGINFO 	29,-,- 		A 		与SIGPWR同义
SIGLOST 	-,-,- 		A 		文件锁丢失
SIGWINCH 	28,28,20 	B 		窗口大小改变(4.3 BSD, Sun)
SIGUNUSED 	-,31,- 		A 		未使用的信号(will be SIGSYS)
信号 29 在 Alpha 上为 SIGINFO / SIGPWR ，在 Sparc 上为 SIGLOST		 */

/**********************	Linux 信号汇总 **********************/
/*	信号列表	shell : $ kill -l
列表中，编号为 1 ~ 31 的信号为传统 UNIX 支持的信号，是 不可靠信号(非实时的)，
编号为 32 ~ 63 的信号 是 后来扩充的，称做 可靠信号(实时信号)。
不可靠信号 和 可靠信号 的区别在于 前者不支持排队，可能会造成信号丢失，而后者不会。
 1) SIGHUP		2) SIGINT	 	3) SIGQUIT	 	4) SIGILL	 	5) SIGTRAP
 6) SIGABRT	 	7) SIGBUS	 	8) SIGFPE	 	9) SIGKILL		10) SIGUSR1
11) SIGSEGV		12) SIGUSR2		13) SIGPIPE		14) SIGALRM		15) SIGTERM
16) SIGSTKFLT	17) SIGCHLD		18) SIGCONT		19) SIGSTOP		20) SIGTSTP
21) SIGTTIN		22) SIGTTOU		23) SIGURG		24) SIGXCPU		25) SIGXFSZ
26) SIGVTALRM	27) SIGPROF		28) SIGWINCH	29) SIGIO		30) SIGPWR
31) SIGSYS		34) SIGRTMIN	35) SIGRTMIN+1	36) SIGRTMIN+2	37) SIGRTMIN+3
38) SIGRTMIN+4	39) SIGRTMIN+5	40) SIGRTMIN+6	41) SIGRTMIN+7	42) SIGRTMIN+8
43) SIGRTMIN+9	44) SIGRTMIN+10	45) SIGRTMIN+11	46) SIGRTMIN+12	47) SIGRTMIN+13
48) SIGRTMIN+14	49) SIGRTMIN+15	50) SIGRTMAX-14	51) SIGRTMAX-13	52) SIGRTMAX-12
53) SIGRTMAX-11	54) SIGRTMAX-10	55) SIGRTMAX-9	56) SIGRTMAX-8	57) SIGRTMAX-7
58) SIGRTMAX-6	59) SIGRTMAX-5	60) SIGRTMAX-4	61) SIGRTMAX-3	62) SIGRTMAX-2
63) SIGRTMAX-1	64) SIGRTMAX
---------------------------------------------------------------------------
信号详解 ：
1) SIGHUP 终止进程，终端线路挂断
 		本信号在用户终端连接(正常或非正常)结束时发出, 通常是在终端的控制进程结束时,
		通知同一session内的各个作业, 这时它们与控制终端不再关联.
2) SIGINT 终止进程，中断进程 Ctrl+C
 		程序终止(interrupt)信号, 在用户键入INTR字符(通常是Ctrl+C)时发出
3) SIGQUIT 和 SIGINT 类似, 但由 QUIT 字符(通常是Ctrl+\)来控制.
 		进程在因收到 SIGQUIT 退出时会产生core文件, 在这个意义上类似于一个程序错误信号.
4) SIGILL 执行了 非法指令. 通常是因为可执行文件本身出现错误, 或者试图执行数据段.
 		堆栈溢出时也有可能产生这个信号.
5) SIGTRAP 由断点指令或其它trap指令产生. 由debugger使用.
6) SIGABRT 执行I/O自陷，程序自己发现错误并调用abort时产生.
6) SIGIOT 跟踪自陷，在PDP-11上由iot指令产生, 在其它机器上和SIGABRT一样.
7) SIGBUS 总线错误，非法地址, 包括内存地址对齐(alignment)出错.
 		eg: 访问一个四个字长的整数，但其地址不是4的倍数.
 		某种特定的硬件异常，通常由内存访问引起
8) SIGFPE 在发生致命的算术运算错误时发出. 不仅包括浮点运算错误,
 		还包括溢出及除数为0等其它所有的算术的错误.
9) SIGKILL 用来立即结束程序（被杀）的运行. 本信号不能被阻塞, 处理和忽略.
10) SIGUSR1 留给用户使用
11) SIGSEGV 试图访问未分配给自己的内存, 或试图往没有写权限的内存地址写数据.
12) SIGUSR2 留给用户使用
13) SIGPIPE Broken pipe
14) SIGALRM 时钟定时信号, 计算的是实际的时间或时钟时间. alarm函数使用该信号.
15) SIGTERM 程序结束(terminate)信号, 与SIGKILL不同的是该信号可以被阻塞和处理.
 		通常用来要求程序自己正常退出. shell命令kill缺省产生这个信号.
17) SIGCHLD 子进程结束时, 父进程会收到这个信号.
18) SIGCONT 让一个停止(stopped)的进程继续执行. 本信号不能被阻塞.
 		可以用一个handler来让程序在由stopped状态变为继续执行时完成特定的工作.
 		例如, 重新显示提示符
19) SIGSTOP 停止(stopped)进程的执行. 注意它和terminate以及interrupt的区别:
 		该进程还未结束, 只是暂停执行. 本信号不能被阻塞, 处理或忽略.
20) SIGTSTP 停止进程的运行, 但该信号可以被处理和忽略.
 		用户键入SUSP字符时(通常是Ctrl+Z)发出这个信号
21) SIGTTIN 当后台作业要从用户终端读数据时, 该作业中的所有进程会收到SIGTTIN信号.
 		缺省时这些进程会停止执行.
22) SIGTTOU 类似于SIGTTIN, 但在写终端(或修改终端模式)时收到.
23) SIGURG 有"紧急"数据或out-of-band数据到达socket时产生.
24) SIGXCPU 超过CPU时间资源限制. 这个限制可以由getrlimit/setrlimit来读取/改变
25) SIGXFSZ 超过文件大小资源限制.
26) SIGVTALRM 虚拟时钟信号. 类似于SIGALRM, 但是计算的是该进程占用的CPU时间.
27) SIGPROF 类似于SIGALRM/SIGVTALRM, 但包括该进程用的CPU时间以及系统调用的时间.
28) SIGWINCH 窗口大小改变时发出.
29) SIGIO 文件描述符准备就绪, 可以开始进行输入/输出操作.
30) SIGPWR 电源（检测）失败
31) SIGSYS 非法的系统调用
---------------------------------------------------------------------------
在以上列出的信号中，程序 不可 捕获、阻塞 或 忽略 的信号有：SIGKILL,SIGSTOP
不能恢复至默认动作的信号有：SIGILL,SIGTRAP
默认会导致进程流产的信号有：
	SIGABRT,SIGBUS,SIGFPE,SIGILL,SIGIOT,SIGQUIT,SIGSEGV,SIGTRAP,SIGXCPU,SIGXFSZ
默认会导致进程退出的信号有：
	SIGALRM,SIGHUP,SIGINT,SIGKILL,SIGPIPE,SIGPOLL,
 	SIGPROF,SIGSYS,SIGTERM,SIGUSR1,SIGUSR2,SIGVTALRM
默认会导致进程停止的信号有：SIGSTOP,SIGTSTP,SIGTTIN,SIGTTOU
默认进程忽略的信号有：SIGCHLD,SIGPWR,SIGURG,SIGWINCH
SIGIO 在 SVR4 是退出，在4.3BSD中是忽略；
SIGCONT 在进程挂起时是继续，否则是忽略，不能被阻塞。
---------------------------------------------------------------------------
信号表 ：
取值 名称 		解释 							默认动作
1 	SIGHUP 		挂起
2 	SIGINT 		中断
3 	SIGQUIT 	退出
4 	SIGILL 		非法指令
5 	SIGTRAP 	断点或陷阱指令
6 	SIGABRT 	abort发出的信号
7 	SIGBUS 		非法内存访问
8 	SIGFPE 		浮点异常
9 	SIGKILL 	被杀信号 						不能被忽略、处理和阻塞
10 	SIGUSR1 	用户信号1
11 	SIGSEGV 	无效内存访问
12 	SIGUSR2 	用户信号2
13 	SIGPIPE 	管道破损，没有读端的管道写数据
14 	SIGALRM 	alarm发出的信号
15 	SIGTERM 	终止信号
16 	SIGSTKFLT 	栈溢出
17 	SIGCHLD 	子进程退出 						默认忽略
18 	SIGCONT 	进程继续
19 	SIGSTOP 	进程停止 						不能被忽略、处理和阻塞
20 	SIGTSTP 	进程停止
21 	SIGTTIN 	进程停止，后台进程从终端读数据时
22 	SIGTTOU 	进程停止，后台进程想终端写数据时
23 	SIGURG 		I/O有紧急数据到达当前进程 			默认忽略
24 	SIGXCPU 	进程的CPU时间片到期
25 	SIGXFSZ 	文件大小的超出上限
26 	SIGVTALRM 	虚拟时钟超时
27 	SIGPROF 	profile时钟超时
28 	SIGWINCH 	窗口大小改变 					默认忽略
29 	SIGIO 		I/O相关
30 	SIGPWR 		关机 							默认忽略
31 	SIGSYS 		系统调用异常 								 */
