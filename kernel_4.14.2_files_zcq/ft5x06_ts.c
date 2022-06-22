/*
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver.
 *
 * Copyright (c) 2010 Focal tech Ltd.
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

/********************************************************************/

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

#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <linux/ratelimit.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/********************************************************************/

#include "ft5x06_ts.h"

/* NOTE: support mulititouch only */
#ifdef CONFIG_ANDROID_PARANOID_NETWORK //CONFIG_STAGING	//in linux Qt single report
#define CONFIG_FT5X0X_MULTITOUCH 1
#endif

#if defined(CONFIG_LVDS_7_0_800x1280)
	int TOUCH_MAX_X  = 1280; //768;//800;//800;//1024;
	int TOUCH_MAX_Y = 800; //1024;//1280;//600;
	#define PRESSURE_MAX   255
#elif defined(CONFIG_LVDS_9_7_1024x768)
	int TOUCH_MAX_X  = 1024; //768;//800;//800;//1024;
	int TOUCH_MAX_Y = 768; //1024;//1280;//600;
	#define PRESSURE_MAX   255
#elif defined(CONFIG_RGB_7_0_1024x600)
	int TOUCH_MAX_X  = 1024; //768;//800;//800;//1024;
	int TOUCH_MAX_Y = 600; //1024;//1280;//600;
	#define PRESSURE_MAX   255
#elif defined(CONFIG_RGB_5_0_800x480)
	int TOUCH_MAX_X  = 800; //768;//800;//800;//1024;
	int TOUCH_MAX_Y = 480; //1024;//1280;//600;
	#define PRESSURE_MAX   255
#endif

#if defined(CONFIG_LVDS_7_0_800x1280)
int type = 1;
#elif defined(CONFIG_LVDS_9_7_1024x768)
int type = 0;
#elif defined(CONFIG_RGB_7_0_1024x600)
int type = 3;
#elif defined(CONFIG_RGB_5_0_800x480)
int type = 4;
#endif

static int swap_xy = 0; /* swap_xy = touch_size */
static int scal_xy = 0; /* LVDS = 1 ; RGB = 0 */

int touch_size = 0; //CONFIG_LVDS_9_7_1024x768 = 1

/********************************************************************/

/*---------------------------------------------------------
 * Chip I/O operations
 */

/*从FT5X06读取多个寄存器数据*/
static int ft5x0x_i2c_rxdata(struct i2c_client *this_client, char *rxdata,
			     int length)
{
	int ret;
	struct i2c_msg msgs[] = {
		/* msgs[0]为发送要读取的首地址 */
		{
			.addr = this_client->addr,	/* ft5x06 地址 */
			.flags = 0,					/* 标记为发送数据 */
			.len = 1,					/* 要读取的数据 rxdata 长度  */
			.buf = rxdata,				/* 要读取的寄存器首地址  */
		},
		/* msgs[1]读取数据 */
		{
			.addr = this_client->addr,	/* ft5x06 地址 */
			.flags = I2C_M_RD,			/* 标记为读取数据 */
			.len = length,		 		/* 要读取的数据 rxdata 长度  */
			.buf = rxdata,				/* 读取数据缓冲区首地址  */
		},
	};

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0) pr_err("%s: i2c read error: %d\n", __func__, ret);
	return ret;
}

/*从FT5X06读取1个寄存器数据*/
static int ft5x0x_read_reg(struct i2c_client *this_client, u8 reg_addr, u8 *pdata)
{
	int ret;
	u8 buf[4] = { 0 };
	struct i2c_msg msgs[] = {
		/* msgs[0]为发送要读取的首地址 */
		{
			.addr = this_client->addr,	/* ft5x06 地址 */
			.flags = 0,					/* 标记为发送数据 */
			.len = 1,					/* 要读取的数据 rxdata 长度  */
			.buf = buf,					/* 要读取的寄存器首地址  */
		},
		/* msgs[1]读取数据 */
		{
			.addr = this_client->addr,	/* ft5x06 地址 */
			.flags = I2C_M_RD,			/* 标记为读取数据 */
			.len = 1,		 			/* 要读取的数据 rxdata 长度  */
			.buf = buf,					/* 读取数据缓冲区首地址  */
		},
	};

	buf[0] = reg_addr; // 要读取的寄存器首地址

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0) {
		pr_err("read reg (0x%02x) error=%d\n", reg_addr, ret);
	} else {
		*pdata = buf[0];
	}

	return ret;
}

/*从FT5X06读取 version */
static int ft5x0x_read_fw_ver(struct ft5x0x_ts_data *ts, unsigned char *val)
{
	int ret;

	*val = 0xff;
	ret = ft5x0x_read_reg(ts->client, FT5X0X_REG_FIRMID, val);
	if (*val == 0x06) {
#if 0
		swap_xy = 1;
		scal_xy = 1;
#endif
	} else {
		/* TODO: Add support for other version */
	}
	// printk("ft5x06_ts: read fw ver ...\n");
	return ret;
}

/*---------------------------------------------------------
 * Touch core support
 */

static void ft5x0x_ts_report(struct ft5x0x_ts_data *ts)
{
	struct ft5x0x_event *event = &ts->event;
	int x, y;
	int i = 0;

#ifdef CONFIG_FT5X0X_MULTITOUCH
	for (i = 0; i < event->touch_point; i++) {
		if (touch_size != 1)
			event->x[i] = ts->screen_max_x - event->x[i];
			//event->y[i] = ts->screen_max_y - event->y[i];
#ifdef CONFIG_PRODUCT_SHENDAO
		event->y[i] = ts->screen_max_y - event->y[i];
#endif

		if (swap_xy) {
			x = event->y[i];
			y = event->x[i];
		} else {
			x = event->x[i];
			y = event->y[i];
		}

		if (scal_xy) {
			x = (x * ts->screen_max_x) / TOUCH_MAX_X;
			y = (y * ts->screen_max_y) / TOUCH_MAX_Y;
		}

		if (0 == touch_size) {
			x = ts->screen_max_x - x;
		}

		printk("x = %d, y = %d\n", x, y);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 64);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
		input_mt_sync(ts->input_dev);

		/*
		//printk("x = %d, y = %d\n", x, y);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);

		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, event->pressure);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);

		input_mt_sync(ts->input_dev);
*/
	}
#else
	if (event->touch_point == 1) {
		if (swap_xy) {
			x = event->y[i];
			y = event->x[i];
		} else {
			x = event->x[i];
			y = event->y[i];
		}

		if (scal_xy) {
			x = (x * ts->screen_max_x) / TOUCH_MAX_X;
			y = (y * ts->screen_max_y) / TOUCH_MAX_Y;
		}

		input_report_abs(ts->input_dev, ABS_X, x);
		input_report_abs(ts->input_dev, ABS_Y, y);
		input_report_abs(ts->input_dev, ABS_PRESSURE, event->pressure);
	}

	/* 上报按键值 */
	//input_event(dev->inputdev, EV_KEY, BTN_TOUCH, 1);
	input_report_key(ts->input_dev, BTN_TOUCH, 1); // 本质就是 input_event 函数
#endif

	input_sync(ts->input_dev);/*上报结束 本质是上报一个同步事件*/
}

static void ft5x0x_ts_release(struct ft5x0x_ts_data *ts)
{
#ifdef CONFIG_FT5X0X_MULTITOUCH
#if 0
	/* NOT needed for ICS */
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
#endif
	input_mt_sync(ts->input_dev);
#else
	input_report_abs(ts->input_dev, ABS_PRESSURE, 0);

	/* 上报按键值 */
	//input_event(dev->inputdev, EV_KEY, BTN_TOUCH, 0);
	input_report_key(ts->input_dev, BTN_TOUCH, 0); // 本质就是 input_event 函数
#endif

	input_sync(ts->input_dev);/*上报结束 本质是上报一个同步事件*/
}

/* 获取 (X,Y) */
static int ft5x0x_read_data(struct ft5x0x_ts_data *ts)
{
	struct ft5x0x_event *event = &ts->event;
	u8 buf[64] = { 0 };
	int ret;

#ifdef CONFIG_FT5X0X_MULTITOUCH
	ret = ft5x0x_i2c_rxdata(ts->client, buf, 63);
#else
	ret = ft5x0x_i2c_rxdata(ts->client, buf, 7);
#endif
	if (ret < 0) {
		printk("%s: read touch data failed, %d\n", __func__, ret);
		return ret;
	}

	memset(event, 0, sizeof(struct ft5x0x_event));

	/* 触摸类型 0x01 : type = buf[1] >> 6;  */

	/* 一个触摸点有6个寄存器来保存触摸值,从0X03开始是触摸
	读取FT5X06触摸点坐标从 0X02 寄存器开始，
	以第一个触摸点为例，寄存器 TOUCH1_XH(地址0X03),各位描述如下：
		bit7:6  Event flag  0:按下 1:释放 2：接触 3：没有事件
		bit5:4  保留
		bit3:0  X轴触摸点的11~8位。
	以第一个触摸点为例，寄存器TOUCH1_YH(地址0X05),各位描述如下：
		bit7:4  Touch ID  触摸ID，表示是哪个触摸点
		bit3:0  Y轴触摸点的11~8位。   */
	event->touch_point = buf[2] & 0x0F;

	if (!event->touch_point) {
		ft5x0x_ts_release(ts);
		return 1;
	}
	//printk("point = %d\n", event->touch_point);
#ifdef CONFIG_FT5X0X_MULTITOUCH
	switch (event->touch_point) {
	case 10:
		event->x[9] = (s16)(buf[57] & 0x0F) << 8 | (s16)buf[58];
		event->y[9] = (s16)(buf[59] & 0x0F) << 8 | (s16)buf[60];
	case 9:
		event->x[8] = (s16)(buf[51] & 0x0F) << 8 | (s16)buf[52];
		event->y[8] = (s16)(buf[53] & 0x0F) << 8 | (s16)buf[54];
	case 8:
		event->x[7] = (s16)(buf[45] & 0x0F) << 8 | (s16)buf[46];
		event->y[7] = (s16)(buf[47] & 0x0F) << 8 | (s16)buf[48];
	case 7:
		event->x[6] = (s16)(buf[39] & 0x0F) << 8 | (s16)buf[40];
		event->y[6] = (s16)(buf[41] & 0x0F) << 8 | (s16)buf[42];
	case 6:
		event->x[5] = (s16)(buf[33] & 0x0F) << 8 | (s16)buf[34];
		event->y[5] = (s16)(buf[35] & 0x0F) << 8 | (s16)buf[36];
	case 5:
		event->x[4] = (s16)(buf[0x1b] & 0x0F) << 8 | (s16)buf[0x1c];
		event->y[4] = (s16)(buf[0x1d] & 0x0F) << 8 | (s16)buf[0x1e];
	case 4:
		event->x[3] = (s16)(buf[0x15] & 0x0F) << 8 | (s16)buf[0x16];
		event->y[3] = (s16)(buf[0x17] & 0x0F) << 8 | (s16)buf[0x18];
		//printk("x:%d, y:%d\n", event->x[3], event->y[3]);
	case 3:
		event->x[2] = (s16)(buf[0x0f] & 0x0F) << 8 | (s16)buf[0x10];
		event->y[2] = (s16)(buf[0x11] & 0x0F) << 8 | (s16)buf[0x12];
		//printk("x:%d, y:%d\n", event->x[2], event->y[2]);
	case 2:
		event->x[1] = (s16)(buf[0x09] & 0x0F) << 8 | (s16)buf[0x0a];
		event->y[1] = (s16)(buf[0x0b] & 0x0F) << 8 | (s16)buf[0x0c];
		//printk("x:%d, y:%d\n", event->x[1], event->y[1]);
	case 1:
		event->x[0] = (s16)(buf[0x03] & 0x0F) << 8 | (s16)buf[0x04];
		event->y[0] = (s16)(buf[0x05] & 0x0F) << 8 | (s16)buf[0x06];
		//printk("x:%d, y:%d\n", event->x[0], event->y[0]);
		break;
	default:
		printk("%s: invalid touch data, %d\n", __func__,
		       event->touch_point);
		return -1;
	}
#else
	/* 一个触摸点有6个寄存器来保存触摸值,从0X03开始是触摸
	读取FT5X06触摸点坐标从 0X02 寄存器开始，
	以第一个触摸点为例，寄存器 TOUCH1_XH(地址0X03),各位描述如下：
		bit7:6  Event flag  0:按下 1:释放 2：接触 3：没有事件
		bit5:4  保留
		bit3:0  X轴触摸点的11~8位。
	以第一个触摸点为例，寄存器TOUCH1_YH(地址0X05),各位描述如下：
		bit7:4  Touch ID  触摸ID，表示是哪个触摸点
		bit3:0  Y轴触摸点的11~8位。   */
	if (event->touch_point == 1) {
		event->x[0] = (s16)(buf[0x03] & 0x0F) << 8 | (s16)buf[0x04];
		event->y[0] = (s16)(buf[0x05] & 0x0F) << 8 | (s16)buf[0x06];
	}
#endif

	event->pressure = 200;

	return 0;
}

/********************************************************************/

/*编写要提交到工作队列中的函数 (延时调度的一个自定义函数)*/
static void ft5x0x_ts_pen_irq_work(struct work_struct *work)
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
	struct ft5x0x_ts_data *ts =
		container_of(work, struct ft5x0x_ts_data, work);

	/* 获取 (X,Y) */
	if (!ft5x0x_read_data(ts)) {
		ft5x0x_ts_report(ts);
	}

	enable_irq(ts->client->irq);
}

/*  中断服务/处理函数 (上半部) */
static irqreturn_t ft5x0x_ts_interrupt(int irq, void *dev_id)
{
	struct ft5x0x_ts_data *ts = dev_id;
    //printk("%s(%d)\n", __FUNCTION__, __LINE__);
	disable_irq_nosync(ts->client->irq);

	if (!work_pending(&ts->work)) {
		/*将工作添加入自己创建的工作队列等待执行
		ft5x0x_ts_pen_irq_work()等待执行 */
		queue_work(ts->queue, &ts->work);
	}
	return IRQ_HANDLED; /*中断已处理*/
}

/** *************** i2c 驱动 结构体 函数 ********************* **/

#ifdef CONFIG_HAS_EARLYSUSPEND
/*悬挂（休眠）驱动*/
static void ft5x0x_ts_suspend(struct early_suspend *handler)
{
#if 1
	struct ft5x0x_ts_data *ts;

	ts = container_of(handler, struct ft5x0x_ts_data, early_suspend);

	disable_irq(ts->client->irq);
	flush_workqueue(ts->queue);
	cancel_work_sync(&ts->work);
	//flush_workqueue(ts->queue);

	//ft5x0x_set_reg(FT5X0X_REG_PMODE, PMODE_HIBERNATE);
#endif
#if 0
	if( IS_ERR_OR_NULL(dc33v_tp) )
	{
		printk( "error on dc33_tp regulator disable : tp_regulator is null\n");
		//return;
	}
	else
	{
		regulator_force_disable(dc33v_tp);
	}
#endif
	printk("ft5x0x_ts: suspended\n");
}

/*驱动恢复后要做什么*/
static void ft5x0x_ts_resume(struct early_suspend *handler)
{
	struct ft5x0x_ts_data *ts;

	ts = container_of(handler, struct ft5x0x_ts_data, early_suspend);
#if 1
	/* Wakeup: output_L --> 100ms --> output_H --> 100ms */
	enable_irq(ts->client->irq);
#endif

	printk("ft5x0x_ts: resumed\n");
}
#endif

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int ft5x0x_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct device_node *np = NULL; /*设备节点*/
	struct ft5x0x_i2c_platform_data *pdata = NULL;
	struct ft5x0x_ts_data *ts = NULL;
	struct input_dev *input_dev = NULL;
	unsigned char val = 0;
	int err = -EINVAL;

	// printk("%s(%d)\n", __FUNCTION__, __LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);	/*分配对齐的内存并清0*/
	if (!ts) {
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	/********************************************************************/

#if defined(CONFIG_LVDS_7_0_800x1280)
	ts->screen_max_x = TOUCH_MAX_X; //768;//800;//800;//1024;
	ts->screen_max_y = TOUCH_MAX_Y; //1024;//1280;//600;
	ts->pressure_max = PRESSURE_MAX;
#elif defined(CONFIG_LVDS_9_7_1024x768)
	ts->screen_max_x = TOUCH_MAX_X; //768;//800;//800;//1024;
	ts->screen_max_y = TOUCH_MAX_Y; //1024;//1280;//600;
	ts->pressure_max = PRESSURE_MAX;
#elif defined(CONFIG_RGB_7_0_1024x600)
	ts->screen_max_x = TOUCH_MAX_X; //768;//800;//800;//1024;
	ts->screen_max_y = TOUCH_MAX_Y; //1024;//1280;//600;
	ts->pressure_max = PRESSURE_MAX; // 255
#elif defined(CONFIG_RGB_5_0_800x480)
	ts->screen_max_x = TOUCH_MAX_X; //768;//800;//800;//1024;
	ts->screen_max_y = TOUCH_MAX_Y; //1024;//1280;//600;
	ts->pressure_max = PRESSURE_MAX;
#endif

	/********************************************************************/

	/* 1、获取设备节点 */
#if 0
	/** 若驱动程序采用 设备树 + platform 方式, 接口： /sys/devices/platform/zcq_beep */
	/*通过节点路径来查找设备树节点，若查找失败，则返回NULL*/
	leddev.node = of_find_node_by_path("/zcq_led"); /*设备节点*/
#else
	np = client->dev.of_node; /*设备节点*/
#endif

	/* 2、 获取设备树中的gpio属性，得到所使用的编号 
	获取设备树中的复位引脚 */
	ts->reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (ts->reset_gpio == -EPROBE_DEFER) return ts->reset_gpio;
	if (ts->reset_gpio < 0) {
		dev_err(&client->dev, "error acquiring reset gpio: %d\n",
			ts->reset_gpio);
		return ts->reset_gpio;
	}

	/* 3、申请 gpio */
	err = devm_gpio_request_one(&client->dev, ts->reset_gpio, 0,
				    "reset-gpios");
	if (err) {
		dev_err(&client->dev, "error requesting reset gpio: %d\n", err);
		return err;
	}

	/* 复位 FT5x06 */
	gpio_set_value(ts->reset_gpio, 0);
	mdelay(200);
	gpio_set_value(ts->reset_gpio, 1); /* 输出高电平，停止复位 */
	msleep(300);

	/********************************************************************/

	/* 	初始化工作队列 _work 
	编写要提交到工作队列中的函数 (延时调度的一个自定义函数) _func
	注，调用完毕后系统会释放此函数，所以如果想再次执行的话，
	就再次调用 schedule_work() 即可。
	另外，内核必须挂载文件系统才可以使用工作队列。
	可理解：工作队列也属于调度，如果内核挂了，他就不调度了，当然就不能用工作队列了。 */
	INIT_WORK(&ts->work, ft5x0x_ts_pen_irq_work);

	ts->client = client;
	ts->gpio_irq = client->irq;
	printk("****** ft5x0x_ts: client irq = %d\n", client->irq);

	/*创建自己的工作队列和工作结构体变量(通常在 open/probe 函数中完成) */
	ts->queue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ts->queue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	/*将保存为设备的私有数据，可通过 i2c_get_clientdata 获取数据*/
	i2c_set_clientdata(client, ts);

	/********************************************************************/

	/** 申请 input_dev 资源 */
	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	ts->input_dev = input_dev;

	/** event设备信息： cat /proc/bus/input/devices 
	I: Bus=0019 Vendor=001f Product=0001 Version=0100
	N: Name=DEVICE_NAME
	P: Phys="pwm/input0"         	*/
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.vendor = 0x12FA;
	input_dev->id.product = 0x2143;
	input_dev->id.version = 0x0100;

	input_dev->name = FT5X0X_NAME; /*设备名字*/
	// input_dev->phys = "i2c/input0";

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
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

#ifdef CONFIG_FT5X0X_MULTITOUCH
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, ts->screen_max_x,
			     0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, ts->screen_max_y,
			     0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 100, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, FT5X0X_PT_MAX, 0,
			     0);

	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
/*
	set_bit(ABS_MT_TRACKING_ID, input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, ts->screen_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, ts->screen_max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, ts->pressure_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, FT5X0X_PT_MAX, 0, 0);
*/
#else
	set_bit(ABS_X, input_dev->absbit);
	set_bit(ABS_Y, input_dev->absbit);
	set_bit(ABS_PRESSURE, input_dev->absbit);
	set_bit(BTN_TOUCH, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_X, 0, ts->screen_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, ts->screen_max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, ts->pressure_max, 0, 0);
#endif

	/** 注册 input 设备 */
	err = input_register_device(input_dev);
	if (err) {
		input_free_device(input_dev);/* 释放 input_dev 资源 */
		dev_err(&client->dev,
			"failed to register input device %s, %d\n",
			dev_name(&client->dev), err);
		goto exit_input_dev_alloc_failed;
	}

	/********************************************************************/

	err = ft5x0x_read_fw_ver(ts, &val);
	if (err < 0) {
		dev_err(&client->dev, "ft5x06_ts: chip not found\n");
		goto exit_irq_request_failed;
	}
	dev_info(&client->dev, "ft5x06_ts: Firmware version 0x%02x\r\n", val);

	/********************************************************************/

	/* 设置中断触发模式为 IRQF_TRIGGER_FALLING 和 IRQF_TRIGGER_RISING，
	也就是 上升沿 和 下降沿 都可以触发中断 */
	/* 在 Linux 内核中要想使用某个中断是需要申请的， request_irq 函数用于申请中断，
	注册中断处理函数，request_irq 函数可能会导致睡眠，
	因此在 中断上下文 或者 其他禁止睡眠 的代码段中 不能使用。
	request_irq 函数会激活(使能)中断，所以不需要我们手动去使能中断。 */
	/** 注册中断处理函数，使能中断 **/
	err = request_irq(client->irq, ft5x0x_ts_interrupt /*中断服务函数 (上半部)*/ , 
			  IRQ_TYPE_EDGE_FALLING /*IRQF_TRIGGER_FALLING*/,
			  "ft5x0x_ts", ts);
	if (err < 0) {
		dev_err(&client->dev, "ft5x06_ts: Request IRQ %d failed, %d\n",
			client->irq, err);
		goto exit_irq_request_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	disable_irq(client->irq);
	ts->early_suspend.level =
		EARLY_SUSPEND_LEVEL_BLANK_SCREEN; //EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ts->early_suspend.suspend = ft5x0x_ts_suspend;
	ts->early_suspend.resume = ft5x0x_ts_resume;
	register_early_suspend(&ts->early_suspend);
	enable_irq(client->irq);
#endif

	/********************************************************************/

	dev_info(&client->dev, "ft5x06_ts: FocalTech ft5x0x TouchScreen initialized\n");
	return 0;

exit_irq_request_failed:
	input_unregister_device(input_dev);

exit_input_dev_alloc_failed:
	cancel_work_sync(&ts->work);/*取消工作队列*/
	destroy_workqueue(ts->queue);/*删除自己的工作队列*/

exit_create_singlethread:
	/*将保存为设备的私有数据，可通过 i2c_get_clientdata 获取数据*/
	i2c_set_clientdata(client, NULL);

exit_no_pdata:
	kfree(ts); /* 释放内存 */

exit_alloc_data_failed:
exit_check_functionality_failed:
	dev_err(&client->dev, "probe ft5x0x TouchScreen failed, %d\n", err);

	return err;
}

/*移除驱动*/
static int ft5x0x_ts_remove(struct i2c_client *client)
{
	/*获取私有数据*/
	struct ft5x0x_ts_data *ts = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif

	if (client->irq) {
		free_irq(client->irq, ts); /*释放中断*/
	}

	cancel_work_sync(&ts->work); /*取消工作队列*/
	destroy_workqueue(ts->queue); /*删除自己的工作队列*/

	input_unregister_device(ts->input_dev); /*注销 input 设备*/
	input_free_device(ts->input_dev); /*释放 input_dev 资源 */

	/*将保存为设备的私有数据，可通过 i2c_get_clientdata 获取数据*/
	i2c_set_clientdata(client, NULL);

	// if (ts->input_dev) kfree(ts->input_dev);/* 释放内存 */
	kfree(ts); /* 释放内存 */
	return 0;
}

/** *************** IIC 匹配列表 ********************* **/
/* 传统匹配方式ID列表 */
static const struct i2c_device_id ft5x0x_ts_id[] = {
	{ FT5X0X_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

/** *************** 设备树 *.dts 匹配列表 ********************* **/

#ifdef CONFIG_OF
static const struct of_device_id ft5x0x_of_match[] = {
	{ .compatible = "edt,edt-ft5406" }, /* 兼容属性 */
	{ /* sentinel */ },
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE(of, ft5x0x_of_match);
#endif

/** *************** i2c 驱动 结构体 ********************* **/

static struct i2c_driver ft5x0x_ts_driver = {
	.probe = ft5x0x_ts_probe,	/*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.remove = ft5x0x_ts_remove, /*移除驱动*/
	.id_table = ft5x0x_ts_id,	/*IIC 匹配列表*/
	// .shutdown = hello_shutdown,			/*关闭驱动*/
	// .suspend = hello_suspend,			/*悬挂（休眠）驱动*/
	// .resume = hello_resume,				/*驱动恢复后要做什么*/
	.driver = {
		.name = FT5X0X_NAME, /* 驱动名字 */
		.owner = THIS_MODULE, /*表示本模块拥有*/
		.of_match_table = of_match_ptr(ft5x0x_of_match), /* 设备树匹配表 */
	},
};

/********************************************************************/

/*驱动模块加载函数*/
static int __init ft5x0x_ts_init(void)
{
	int ret;

	if (0x00 == type) //9.7 CONFIG_LVDS_9_7_1024x768
	{
#ifdef CONFIG_VT //for Ubuntu
		touch_size = 1;
		scal_xy = 1;
#else
		touch_size = 1; //0;
		scal_xy = 1;
#endif
	} else if (0x01 == type) //7.0 CONFIG_LVDS_7_0_800x1280
	{
#ifdef CONFIG_VT //for Ubuntu
		scal_xy = 1;
		touch_size = 0;
#else
		touch_size = 0; //1;
#endif
	} else if (0x02 == type) //4.3
	{
		;
	} else if (0x03 == type) //1024x600 CONFIG_RGB_7_0_1024x600
	{
		touch_size = 0; //0;
		scal_xy = 0;
	} else if (0x04 == type) //800x480 CONFIG_RGB_5_0_800x480
	{
		touch_size = 0;
		scal_xy = 0;
	}

	if (1 == touch_size) {
		swap_xy = 1;
	} else {
		swap_xy = 0;
	}

	return i2c_add_driver(&ft5x0x_ts_driver); /*i2c 驱动注册/添加*/
}

/*驱动模块卸载函数*/
static void __exit ft5x0x_ts_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	// printk(KERN_EMERG "leddriver_exit\n");
	i2c_del_driver(&ft5x0x_ts_driver); /*i2c 驱动注销/删除*/
}

module_init(ft5x0x_ts_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(ft5x0x_ts_exit); /*卸载驱动时运行的函数， 如 rmmod*/

/********************************************************************/

MODULE_LICENSE("GPL"); /*Dual BSD/GPL 声明是开源的，没有内核版本限制*/
MODULE_AUTHOR("zcq"); /*声明作者*/
MODULE_DESCRIPTION("zcq ft5x0x TouchScreen driver");

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
	6、删除自己的工作队列 (通常在 close 函数中删除)
		destroy_workqueue(p_queue);

*/
