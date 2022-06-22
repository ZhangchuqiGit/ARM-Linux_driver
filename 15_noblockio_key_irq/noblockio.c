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
#include <linux/timer.h>
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

#include <linux/wait.h>
#include <linux/poll.h>

#define IMX6UIRQ_CNT        1            /* 设备号个数 	*/
#define IMX6UIRQ_NAME        "blockio"    /* 名字 		*/

#define KEY0VALUE            0X01        /* KEY0按键值 	*/
#define INVAKEY              0XFF        /* 无效的按键值 */

#define KEY_NUM                1            /* 按键数量 	*/

/* 寄存器物理地址：设备树方式不需要 */

/* 映射后的寄存器虚拟地址指针：princtl 方式不需要 */

/** 中断IO描述结构体 */
struct irq_keydesc {
	int gpio;                        // key 所使用的 Gled_switch PIO 编号
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

	wait_queue_head_t r_wait; /* 读等待队列头
	阻塞访问最大的好处就是当设备文件不可操作的时候进程可以进入休眠态，这样可以将 CPU 资源让出来。
 但是，当设备文件可以操作的时候就必须唤醒进程，一般在中断函数里面完成唤醒工作。
 Linux 内核提供了等待队列(wait queue)来实现阻塞进程的唤醒工作  */
};

struct imx6uirq_dev imx6uirq;    /* irq设备 */

/* @description		: 中断服务函数，开启定时器		
 *				  	  定时器用于按键消抖。
 * @param - irq 	: 中断号 
 * @param - dev_id	: 设备结构。
 * @return 			: 中断执行结果
 */
static irqreturn_t key0_handler(int irq, void *dev_id)  // 中断服务函数 (上半部)
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

	/** 唤醒进程 */
	if (atomic_read(&dev->releasekey)) {    /* 完成一次按键过程 */
		/* wake_up(&dev->r_wait); // 唤醒当前进程 */
		wake_up_interruptible(&dev->r_wait); // 唤醒当前进程
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
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *) filp->private_data;  /* 获取私有数据 */

	if (filp->f_flags & O_NONBLOCK) { /* 非阻塞访问 */
		if (atomic_read(&dev->releasekey) == 0)    /* 没有按键按下，返回 -EAGAIN */
			return -EAGAIN;
	} else {                            /* 阻塞访问 */
#if 1
		/** 加入等待队列，等待被唤醒,也就是有按键按下 */
		ret = wait_event_interruptible(dev->r_wait, atomic_read(&dev->releasekey));
		if (ret) {
			return ret;
		}
#else
		DECLARE_WAITQUEUE(wait, current);    /* 定义一个等待队列 */
		if (atomic_read(&dev->releasekey) == 0) {    /* 没有按键按下 */
			add_wait_queue(&dev->r_wait, &wait);    /* 将等待队列添加到等待队列头 */

			__set_current_state(TASK_INTERRUPTIBLE);/** 设置当前进程的状态为 TASK_INTERRUPTIBLE */
			schedule();                    /** 进行一次任务切换，当前进程就会进入到休眠态 */
			if (signal_pending(current)) {            /* 判断是否为信号引起的唤醒 */
				set_current_state(TASK_RUNNING);        /* 设置任务为运行态 */
				remove_wait_queue(&dev->r_wait, &wait);    /* 将等待队列移除 */
				return -ERESTARTSYS;
			}
			__set_current_state(TASK_RUNNING);      /** 将当前任务设置为运行状态 */

			remove_wait_queue(&dev->r_wait, &wait);    /* 将对应的队列项从等待队列头删除 */
		}
#endif
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
		atomic_set(&dev->releasekey, 0);  // 按下标志清零
	} else {
		goto data_error;
	}
	return ret;

data_error:
	return -EINVAL;
}

/*
 * @description     : poll函数，用于处理 非 阻塞访问
 * @param - filp    : 要打开的设备文件(文件描述符)
 * @param - wait    : 等待列表(poll_table)
 * @return          : 设备或者资源状态，
 */
/** 当应用程序调用 select、 epoll 或 poll 函数的时候，设备驱动程序中的 poll 函数就会执行，
因此需要在设备驱动程序中编写 poll 函数。 */
unsigned int imx6uirq_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *) filp->private_data;  /* 获取私有数据 */

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

/* 设备操作函数 */
static struct file_operations imx6uirq_fops = {
		.owner = THIS_MODULE,
		.open = imx6uirq_open,
		.read = imx6uirq_read,
		.poll = imx6uirq_poll,
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

/* 	linux c++ select/poll/epoll 的个人见解
Select
	通过设置或者检查存放在数据结构 fd_set 中的标志位的来鉴别事件。Fd_set是一个输入输出参数，用户通过设置(FD_SET)相应的标志位标识关注的描述符，经内核拷贝到内核空间；内核根据输入fd_set 扫描对应的描述符，新建另一fd_set标识活跃的描述符，扫描完毕后将含有活跃描述符的fd_set 拷贝到用户空间。用户检查(FD_ISSET)内核输出的fd_set 确定活跃的描述符。（进程的fd的分配都是从3开始的，0、1、2已经用作标准输入，输出和错误？而fd_set的每一位对应一个fd。）
Poll
	poll与select相似，poll用结构体标识一个fd，将关注的事件合并用events位标识。用户将关注的fd对应的结构体添加到数组中，并从用户空间拷贝到内核空间。内核轮询数组标识的fd，如有就绪fd这设置对应结构体的revents标志位，并继续遍历，最后将结构体数组拷贝到用户空间，用户检查revents标识并进行处理。如遍历完仍未有fd就绪，则挂起当前进程，直到再次被调度，被唤醒后再次遍历结构数组。采用“水平触发”的侦测方式，如检测到fd就绪后，没处理，那么下次epoll时会再次报告该fd。
Epoll
	Epoll在用户空间和内核使用mmap方式传递数据（epoll_ctl函数添加关注的fd），避免了复制开销。Epoll使用就绪通知的模式，内核将就绪的fd添加到就绪队列中并返回，epoll_wait收到的都是就绪的fd。支持两种侦测方式，“边沿触发”和“边沿触发”，当采用“水平触发”（默认）的侦测方式，有poll相同的问题。
---------------------------------------------------------------------------
进程管理最大文件描述符
Select 由宏定义FD_SETSIZE决定，用一个unsigned long数组表示。
 	一般FD_SETSIZE定义位1024，可以修改FD_SETSIZE来改变select管理描述符的数量。
Poll 基于链表存储，无上限，但受内存上限限制
Epoll 同 poll。
---------------------------------------------------------------------------
效率
Select 内核和用户空间使用内核拷贝的方式交互数据，无论内核和用户空间，
 	都需要轮询整个fd_set,当随管理的fd增加时，效率会呈线性下降。
Poll 同 select
Epoll 没有内核拷贝，而且只返回就绪的fd。在侦听大量不活跃的fd时，效率比较高。
 	但在侦听少量活跃的fd时，性能不如前两者。因为epoll使用了复杂算法。
---------------------------------------------------------------------------
IO多路复用是指内核一旦发现过程指定的一个或者多个IO条件筹备读取，它就告诉该过程。
IO多路复用实用如下场合：
    当客户解决多个描述符时（个别是交互式输出和网络套接口），必须应用I/O复用。
    当一个客户同时解决多个套接口时，而这种状况是可能的，但很少呈现。
    如果一个TCP服务器既要解决监听套接口，又要解决已连贯套接口，个别也要用到I/O复用。
    如果一个服务器即要解决TCP，又要解决UDP，个别要应用I/O复用。
    如果一个服务器要解决多个服务或多个协定，个别要应用I/O复用。
---------------------------------------------------------------------------
 （一）select()函数
原型如下：
1 int select(int fdsp1, fd_set *readfds, fd_set *writefds, fd_set *errorfds, const struct timeval *timeout);

各个参数含义如下：
    int fdsp1:最大描述符值 + 1
    fd_set *readfds:对可读感兴趣的描述符集
    fd_set *writefds:对可写感兴趣的描述符集
    fd_set *errorfds:对出错感兴趣的描述符集
    struct timeval *timeout:超时时间（注意：对于linux系统，此参数没有const限制，每次select调用完毕timeout的值都被修改为剩余时间，而unix系统则不会改变timeout值）

select函数会在发生以下情况时返回：
    readfds集合中有描述符可读
    writefds集合中有描述符可写
    errorfds集合中有描述符遇到错误条件
    指定的超时时间timeout到了

当select返回时，描述符集合将被修改以指示哪些个描述符正处于可读、可写或有错误状态。可以用FD_ISSET宏对描述符进行测试以找到状态变化的描述符。如果select因为超时而返回的话，所有的描述符集合都将被清空。
select函数返回状态发生变化的描述符总数。返回0意味着超时。失败则返回-1并设置errno。可能出现的错误有：EBADF（无效描述符）、EINTR（因终端而返回）、EINVAL（nfds或timeout取值错误）。

设置描述符集合通常用如下几个宏定义：
1 FD_ZERO(fd_set *fdset);               清除fdset中的所有位
2 FD_SET(int fd, fd_set *fdset);        在fd_set中打开fd的位
3 FD_CLR(int fd, fd_set *fdset);        在fd_set中关闭fd的位
4 int FD_ISSET(int fd, fd_set *fdset);  fdset中fd的作用是什么？

如:
1 fd_set read_set;
2 FD_ZERO(&read_set);                   初始化集合：所有位均关闭
3 FD_SET(1, &read_set);                 开启fd 1
4 FD_SET(4, &read_set);                 为fd 4开启位
5 FD_SET(5, &read_set);                 开启fd 5

当select返回的时候，rset位都将被置0，除了那些有变化的fd位。

当发生如下情况时认为是可读的：
1.socket的receive buffer中的字节数大于socket的receive buffer的low-water mark属性值。
 （low-water mark值类似于分水岭，当receive buffer中的字节数小于low-water mark值的时候，认为socket还不可读，只有当receive buffer中的字节数达到一定量的时候才认为socket可读）
2.连接半关闭（读关闭，即收到对端发来的FIN包）
3.发生变化的描述符是被动套接字，而连接的三路握手完成的数量大于0，即有新的TCP连接建立
4.描述符发生错误，如果调用read系统调用读套接字的话会返回-1。

当发生如下情况时认为是可写的：
1.socket的send buffer中的字节数大于socket的send buffer的low-water mark属性值以及socket已经连接或者不需要连接（如UDP）。
2.写半连接关闭，调用write函数将产生SIGPIPE
3.描述符发生错误，如果调用write系统调用写套接字的话会返回-1。

注意：
1.select默认能处理的描述符数量是有上限的，为FD_SETSIZE的大小。
2.对于timeout参数，如果置为NULL，则表示wait forever；若timeout->tv_sec = timeout->tv_usec = 0，则表示do not wait at all；否则指定等待时间。
3.如果使用select处理多个套接字，那么需要使用一个数组（也可以是其他结构）来记录各个描述符的状态。而使用poll则不需要，下面看poll函数。
---------------------------------------------------------------------------
（二）poll()函数
原型如下：
1 int poll(struct pollfd *fdarray, unsigned long nfds, int timeout);

各参数含义如下：
struct pollfd *fdarray:		一个结构体，用来保存各个描述符的相关状态。
unsigned long nfds:			fdarray 数组的大小，即里面包含有效成员的数量。
int timeout:				设定的超时时间。（以毫秒为单位）
---------------------------------------------------------------------------
参数 timeout 的设置如下：
	INFTIM (-1):     wait forever
	0   :            return immediately, do not block
	>0     :         wait specified number of milliseconds
---------------------------------------------------------------------------
poll()函数 返回值 ：
-1：有错误产生
0：超时时间到，而且没有描述符有状态变化
>0：有状态变化的描述符个数
---------------------------------------------------------------------------
着重讲 fdarray 数组，因为这是它和select()函数主要的不同的地方：
pollfd 的结构如下：
	struct pollfd {
	    int fd;            要检查的描述符
	    short events;      fd 感兴趣 的事件 / 监听的事件
	    short revents;     fd 发生 的事件 / poll()返回时描述符的返回状态
	 };
---------------------------------------------------------------------------
其实poll()和select()函数要处理的问题是相同的，只不过是不同组织在几乎相同时刻同时推出的，因此才同时保留了下来。select()函数把可读描述符、可写描述符、错误描述符分在了三个集合里，这三个集合都是用bit位来标记一个描述符，一旦有若干个描述符状态发生变化，那么它将被置位，而其他没有发生变化的描述符的bit位将被clear，也就是说select()的readset、writeset、errorset是一个value-result类型，通过它们传值，而也通过它们返回结果。这样的一个坏处是每次重新select 的时候对集合必须重新赋值。而poll()函数则与select()采用的方式不同，它通过一个结构数组保存各个描述符的状态，每个结构体第一项fd代表描述符，第二项代表要监听的事件，也就是感兴趣的事件，而第三项代表poll()返回时描述符的返回状态。合法状态如下：
---------------------------------------------------------------------------
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
---------------------------------------------------------------------------
POLLIN | POLLPRI 等价与 select() 的可读事件；
POLLOUT | POLLWRBAND 等价与 select() 的可写事件；
POLLIN 等价与 POLLRDNORM | POLLRDBAND，而 POLLOUT 等价于 POLLWRBAND。
如果你对一个描述符的可读事件和可写事件以及错误等事件均感兴趣那么你应该都进行相应的设置。
---------------------------------------------------------------------------
（三）epoll()函数
// 创建 epoll 模型，指定 fd 监听数量的最大值，返回描述符 epoll_fd，
int epoll_create (int size);
参数：size 指定 fd 监听数量的最大值，注意是 fd 个数，不是 fd 最大值。
返回：成功返回 描述符 epoll_fd，失败返回 -1，见 errno。
备注：创建 epoll 句柄会占用一个 fd 值，在使用完 epoll 后，
 	必须调用 close() 关闭，否则可能导致 fd 被耗尽。
--------------------------------
int epoll_create1 (int flags);
---------------------------------------------------------------------------
 // 注册管理 epoll 模型，事件注册函数，比如 添加描述符 等：
int epoll_ctl(int epoll_fd, int op, int fd, struct epoll_event *event);
参数：
 epoll_fd：epoll_create()返回的描述符。
 op：操作常量，用三个宏表示表示 动作：
				EPOLL_CTL_ADD 	注册新的 fd 到 epfd()中
				EPOLL_CTL_MOD 	修改已经注册的 fd 的监听事件
				EPOLL_CTL_DEL 	从 epfd 中删除一个 fd
 fd：操作目标，是需要监听的（套接字）描述符。
 event：数据结构，感兴趣的事件以及任何关联的用户数据。
 返回：成功返回 0，错误返回 -1，见 errno。
---------------------------------------------------------------------------
 event{} 是设定监听事件的结构体，数据结构如下：
typedef union epoll_data
{
  	void 	 *ptr;
  	int 	 fd;  	//操作目标，是需要监听的（套接字）描述符
  	uint32_t u32;
  	uint64_t u64;
} epoll_data_t;
struct epoll_event
{
  	uint32_t 	 events;	感兴趣的事件
	epoll_data_t data;		用户数据
} __EPOLL_PACKED;
---------------------------------------------------------------------------
 感兴趣的事件 events 可以是以下几个宏的集合：
EPOLLIN ：表示监听的文件描述符 可读（包括对端 socket 正常关闭）；
EPOLLOUT：表示监听的文件描述符 可写；
EPOLLPRI：表示监听的文件描述符 有紧急的数据 可读（这里应该表示有 带外数据 到来）；
EPOLLERR：表示监听的文件描述符 发生错误；
EPOLLHUP：表示监听的文件描述符 被挂断；
EPOLLONESHOT：事件只监听一次，当监听完这次事件之后，就会把这个 fd 从 epoll 的队列中删除。
		如果还需要继续监听这个 socket 的话，需要再次把这个 fd 加入到 epoll 队列里。
EPOLLET： 将 epoll 设为 边缘触发(Edge Triggered)模式，
		这是相对于 水平触发(Level Triggered)来说的。
-----------------------------
 关于 边缘触发(ET, Edge Triggered)、水平触发(LT, Level Triggered)两种工作模式：
 	边缘触发 ET 是缺省的工作方式，并且同时支持 block 和 no-block socket。
在这种做法中，内核告诉你一个文件描述符是否就绪了，然后你可以对这个就绪的 fd 进行 IO 操作。
如果你不作任何操作，内核还是会继续通知你的，所以，这种模式编程出错误可能性要小一点。
传统的 select/poll 都是这种模型的代表。
 	水平触发 LT 是高速工作方式，只支持 no-block socket。
在这种模式下，当描述符从未就绪变为就绪时，内核通过 epoll 告诉你。
然后它会假设你知道文件描述符已经就绪，并且不会再为那个文件描述符发送更多的就绪通知
（仅仅发送一次），直到你做了某些操作导致那个文件描述符不再为就绪状态了
（如你在发送、接收 或 接收请求，或者发送接收的数据少于一定量时导致了一个 EWOULDBLOCK 错误）。
 但是请注意，如果一直不对这个 fd 作 IO操作（从而导致它再次变成未就绪），
内核不会发送更多的通知（only once），不过在 TCP 协议中，
ET 模式的加速效用仍需要更多的 bench mark 确认。
-----------------------------
 ET 和 LT 的区别：
 	LT 事件不会丢弃，只要 有数据 可以让用户读，则不断的通知你。
 	LT 模式是 只要 有事件、有数据 没有处理 就会一直通知下去的。
	LT 模式下只要某个 fd 处于 readable/writable 状态，
 无论什么时候进行 epoll_wait() 都会返回该 fd；
 	ET 则只在事件 发生之时 通知。
 	ET 则只在高低电平变换时（即状态从1到0或者0到1）触发。
 	ET 模式下只有某个 fd 从 unreadable 变为 readable 或从 unwritable 变为 writable 时，
 epoll_wait() 才会返回该 fd。
 	ET 模式仅当 状态发生变化 的时候才获得通知，这里所谓的状态的变化
 并不包括缓冲区中还有未处理的数据，也就是说，如果要采用 ET 模式，
 需要一直 read/write 直到出错为止，很多人反映为什么采用 ET 模式只接收了一部分数据
 就再也得不到通知了，大多因为这样。
-----------------------------
在 epoll 的 ET 模式下，正确的读写方式为：
         读：只要可读，就一直读，直到返回 0，或者 errno = EAGAIN
         写：只要可写，就一直写，直到数据发送完，或者 errno = EAGAIN
---------------------------------------------------------------------------
 // 等待一个 epoll 实例上 I/O 事件发生：
int epoll_wait (int epoll_fd, struct epoll_event *event, int maxevents,
 				int timeout );
参数：
 epoll_fd：epoll_create()返回的描述符。
 event：数据结构，已触发的事件以及任何关联的用户数据。
 maxevents：要求的最大事件数。(监听数量的最大值)
 timeout：指定毫秒为单位的最大等待时间（ 0 会立即返回，-1 会永久阻塞）。
返回：成功返回 需要处理的 事件数目，返回 0 表示已超时，错误返回 -1，见 errno。
-----------------------------
int epoll_pwait(int epoll_fd, struct epoll_event *events, int maxevents,
 				int timeout, const sigset_t *sigmask );
 与 epoll_wait 等待相同，但是线程的信号掩码暂时并用作为参数提供的原子替换。   */


