/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2008 Intel Corporation
 * Author: Matthew Wilcox <willy@linux.intel.com>
 *
 * Please see kernel/locking/semaphore.c for documentation of these functions
 */
#ifndef __LINUX_SEMAPHORE_H
#define __LINUX_SEMAPHORE_H

#include <linux/list.h>
#include <linux/spinlock.h>

/* Please don't access any members of this structure directly */
struct semaphore {
	raw_spinlock_t		lock;
	unsigned int		count;
	struct list_head	wait_list;
};

#define __SEMAPHORE_INITIALIZER(name, n)				\
{									\
	.lock		= __RAW_SPIN_LOCK_UNLOCKED((name).lock),	\
	.count		= n,						\
	.wait_list	= LIST_HEAD_INIT((name).wait_list),		\
}

#define DEFINE_SEMAPHORE(name)	\
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)

/* 初始化信号量 sem，设置信号量值为 val。 */
static inline void sema_init(struct semaphore *sem, int val)
{
	static struct lock_class_key __key;
	*sem = (struct semaphore) __SEMAPHORE_INITIALIZER(*sem, val);
	lockdep_init_map(&sem->lock.dep_map, "semaphore->lock", &__key, 0);
}

/*获取信号量，因为会导致休眠且不能被信号打断，因此不能在中断中使用。*/
extern void down(struct semaphore *sem);

/*获取信号量，和 down 类似，而使用此函数进入休眠以后是可以被信号打断的。*/
extern int __must_check down_interruptible(struct semaphore *sem);

extern int __must_check down_killable(struct semaphore *sem);

/*尝试获取信号量，如果能获取到信号量就获取，并且返回 0。
如果不能就返回非 0，并且不会进入休眠。*/
extern int __must_check down_trylock(struct semaphore *sem);

extern int __must_check down_timeout(struct semaphore *sem, long jiffies);

extern void up(struct semaphore *sem);/*释放信号量*/

#endif /* __LINUX_SEMAPHORE_H */


/** 信号量（semaphore）是一种提供不同进程之间或者一个给定进程不同线程之间的同步。
 	POSIX、SystemV 信号随内核持续。
 	Linux操作系统中，POSIX 有名 信号量 创建在虚拟文件系统中，一般挂载在 /dev/shm，
 其名字以 sem.somename 的形式存在。
 	信号量初始化的值的大小一般用于表示可用资源的数（例如缓冲区大小，之后代码中体现）；
 如果初始化为 1，则称之二值信号量，二值信号量的功能就有点像互斥锁了。
 不同的是：互斥锁的加锁和解锁必须在同一线程执行，而信号量的挂出却不必由执行等待操作的线程执行。
--------------------------------------------------------------------------
 有名信号量（基于路径名 /dev/sem/sem.zcq）：通常用于 不同进程之间的同步
 无名信号量（基于内存的信号量）：通常用于 一个给定进程的 不同线程之间的同步
--------------------------------------------------------------------------
 注意：fork()的子进程，通常不共享父进程的内存空间，
 子进程是在父进程的副本上启动的，它跟共享内存区不是一回事。
 不同进程之间的同步 若使用 无名信号量（基于内存的信号量），要考虑指针或地址的关联。
--------------------------------------------------------------------------
 一个信号量的最大值  SEM_VALUE_MAX  (2147483647)  **/

/**	信号量 广泛用于进程或线程间的同步和互斥，信号量本质上是一个非负的整数计数器，
它被用来控制对公共资源的访问。可根据操作信号量值的结果判断是否对公共资源具有访问的权限，
当信号量值大于 0 时，则可以访问，否则将阻塞。PV 原语是对信号量的操作，
一次 P (wait) 操作使信号量减１，一次 V (post) 操作使信号量加１。 **/
