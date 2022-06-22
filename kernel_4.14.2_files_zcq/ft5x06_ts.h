/*
 * include/linux/ft5x06_touch.h
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*	tsc2007	驱动使用内核自带的 drivers/input/touchscreen/tsc2007_core.c
	ft5x06 	驱动需要移植 位置： drivers/input/touchscreen/ft5x06_ts.c
	gt911 	驱动需要移植 位置： drivers/input/touchscreen/gt911.c	
	LCD 背光电源初始化驱动 drivers/char/power_ctrl.c
移植所需驱动会放在 8_source 文件夹中。
------------------------------------------------------------------------
使用 “ vim drivers/input/touchscreen/Kconfig ” 修改 Kconfig ...
使用 “ vim drivers/input/touchscreen/Makefile ” 修改 Makefile ...
打开图形配置界面, 使用.config 替换掉 topeet4412_defconfig ... 
教程见 《4412_内核移植笔记.pdf》  	*/

#ifndef __LINUX_FT5X0X_TS_H__
#define __LINUX_FT5X0X_TS_H__


#define FT5X0X_NAME				"ft5x0x_ts"
#define FT5X0X_PT_MAX			10  //5

#if 0
#define SCREEN_MAX_X			480
#define SCREEN_MAX_Y			272
#define PRESS_MAX				255
#endif


//---------------------------------------------------------

enum ft5x0x_ts_regs {
	FT5X0X_REG_THGROUP			= 0x80,
	FT5X0X_REG_THPEAK			= 0x81,
//	FT5X0X_REG_THCAL					= 0x82,
//	FT5X0X_REG_THWATER					= 0x83,
//	FT5X0X_REG_THTEMP					= 0x84,
//	FT5X0X_REG_THDIFF					= 0x85,
//	FT5X0X_REG_CTRL						= 0x86,
	FT5X0X_REG_TIMEENTERMONITOR	= 0x87,
	FT5X0X_REG_PERIODACTIVE		= 0x88,
	FT5X0X_REG_PERIODMONITOR	= 0x89,
//	FT5X0X_REG_HEIGHT_B					= 0x8a,
//	FT5X0X_REG_MAX_FRAME				= 0x8b,
//	FT5X0X_REG_DIST_MOVE				= 0x8c,
//	FT5X0X_REG_DIST_POINT				= 0x8d,
//	FT5X0X_REG_FEG_FRAME				= 0x8e,
//	FT5X0X_REG_SINGLE_CLICK_OFFSET		= 0x8f,
//	FT5X0X_REG_DOUBLE_CLICK_TIME_MIN	= 0x90,
//	FT5X0X_REG_SINGLE_CLICK_TIME		= 0x91,
//	FT5X0X_REG_LEFT_RIGHT_OFFSET		= 0x92,
//	FT5X0X_REG_UP_DOWN_OFFSET			= 0x93,
//	FT5X0X_REG_DISTANCE_LEFT_RIGHT		= 0x94,
//	FT5X0X_REG_DISTANCE_UP_DOWN			= 0x95,
//	FT5X0X_REG_ZOOM_DIS_SQR				= 0x96,
//	FT5X0X_REG_RADIAN_VALUE				= 0x97,
//	FT5X0X_REG_MAX_X_HIGH				= 0x98,
//	FT5X0X_REG_MAX_X_LOW				= 0x99,
//	FT5X0X_REG_MAX_Y_HIGH				= 0x9a,
//	FT5X0X_REG_MAX_Y_LOW				= 0x9b,
//	FT5X0X_REG_K_X_HIGH					= 0x9c,
//	FT5X0X_REG_K_X_LOW					= 0x9d,
//	FT5X0X_REG_K_Y_HIGH					= 0x9e,
//	FT5X0X_REG_K_Y_LOW					= 0x9f,
	FT5X0X_REG_AUTO_CLB_MODE	= 0xa0,
//	FT5X0X_REG_LIB_VERSION_H			= 0xa1,
//	FT5X0X_REG_LIB_VERSION_L			= 0xa2,
//	FT5X0X_REG_CIPHER					= 0xa3,
//	FT5X0X_REG_MODE						= 0xa4,
	FT5X0X_REG_PMODE			= 0xa5,			/* Power Consume Mode */
	FT5X0X_REG_FIRMID			= 0xa6,
//	FT5X0X_REG_STATE					= 0xa7,
//	FT5X0X_REG_FT5201ID					= 0xa8,
	FT5X0X_REG_ERR				= 0xa9,
	FT5X0X_REG_CLB				= 0xaa,
};

// FT5X0X_REG_PMODE
#define PMODE_ACTIVE			0x00
#define PMODE_MONITOR			0x01
#define PMODE_STANDBY			0x02
#define PMODE_HIBERNATE			0x03


//---------------------------------------------------------

struct ft5x0x_event {
	int touch_point;
	u16 x[FT5X0X_PT_MAX];
	u16 y[FT5X0X_PT_MAX];
	u16 pressure;
};

struct ft5x0x_ts_data {
	struct input_dev *input_dev;
	struct ft5x0x_event event;
	uint32_t gpio_irq;
	uint32_t gpio_wakeup;
	uint32_t gpio_reset;
	int screen_max_x;
	int screen_max_y;
	int pressure_max;
	struct work_struct work;
	struct workqueue_struct *queue;
    struct i2c_client *client;
	int reset_gpio;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};


#endif	// __LINUX_FT5X0X_TS_H__


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
		queue_w

*/

