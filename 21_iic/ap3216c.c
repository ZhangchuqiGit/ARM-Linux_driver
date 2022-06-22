#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
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
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>

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

#include <linux/i2c.h>
#include "ap3216creg.h" // 集成光传感器，距离传感器，红外LED的芯片
/** 学习 Linux 下的 I2C 驱动框架，按照指定的框架去编写 I2C 设备驱动。
 本章同样以 I.MX6U-ALPHA 开发板上的 AP3216C 这个三合一环境光传感器为例。
 	platform 是虚拟出来的一条总线，目的是为了实现总线、设备、驱动框架。
 对于 I2C 而言，不需要虚拟出一条总线，直接使用 I2C 总线即可。
 I2C 总线驱动重点是 I2C 适配器(也就是 SOC 的 I2C 接口控制器)驱动，
 这里要用到两个重要的数据结构： i2c_adapter 和 i2c_algorithm，
 Linux 内核将 SOC 的 I2C 适配器(控制器) 抽象成 i2c_adapter，见 include/linux/i2c.h 文件。
	一般 SOC 的 I2C 总线驱动都是由半导体厂商编写的，
 比如 I.MX6U 的 I2C 适配器驱动 NXP 已经编写好了，这个不需要用户去编写。
 因此 I2C 总线驱动对我们这些 SOC 使用者来说是被屏蔽掉的，我们只要专注于 I2C 设备驱动即可。
	I2C 设备驱动重点关注两个数据结构： i2c_client 和 i2c_driver，总线、设备和驱动模型。
 还剩下设备和驱动， i2c_client 就是描述设备信息的，类似于 platform_driver。
	I2C 设备和驱动的匹配过程是由 I2C 核心来完成的， drivers/i2c/i2c-core.c。
1、 i2c_adapter 注册/注销函数
	int i2c_add_adapter(struct i2c_adapter *adapter)
	int i2c_add_numbered_adapter(struct i2c_adapter *adap)
	void i2c_del_adapter(struct i2c_adapter * adap)
2、 i2c_driver 注册/注销函数
	int i2c_register_driver(struct module *owner, struct i2c_driver *driver)
	int i2c_add_driver (struct i2c_driver *driver)
	void i2c_del_driver(struct i2c_driver *driver)
  	Linux 下的 I2C 驱动框架，重点分为 I2C 适配器驱动和 I2C 设备驱动，
	其中 I2C 适配器驱动就是 SOC 的 I2C 控制器驱动。
I2C 设备驱动是需要用户根据不同的 I2C 设备去编写，而 I2C 适配器驱动一般都是 SOC 厂商去编写的，
比如 NXP 就编写好了 I.MX6U 的 I2C 适配器驱动。  */

#define AP3216C_CNT            1         /* 设备号个数*/
#define AP3216C_NAME        "ap3216c"    /* 设备名字	*/

struct ap3216c_dev {
	dev_t devid;            /* 设备号 	 */
	struct cdev cdev;        /* cdev 	*/
	struct class *class;    /* 类 		*/
	struct device *device;    /* 设备 	 */
	struct device_node *nd; /* 设备节点 */
	int major;            /* 主设备号 */

	void *private_data;    /* 私有数据 */
	unsigned short ir, als, ps; // 三个光传感器数据
	unsigned char ps_mode;
};

static struct ap3216c_dev ap3216cdev;

/*
 * @description	: 从ap3216c读取多个寄存器数据:实现多字节读取
 AP3216C 好像不支持连续多字节读取，此函数在测试其他 I2C 设备的时候可以实现多给字节连续读取，
 但是在 AP3216C 上不能连续读取多个字节。不过读取一个字节没有问题的
 * @param - dev:  ap3216c设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int ap3216c_read_regs(struct ap3216c_dev *dev, u8 reg, void *val, int len)
{
	int ret;
	struct i2c_msg msg[2];
	struct i2c_client *client = (struct i2c_client *) dev->private_data;/* 私有数据 */

	/* msg[0]为 发送 要读取的首地址 */
	msg[0].addr = client->addr;            /* ap3216c地址 */
	msg[0].flags = 0;                    /* 0=写数据 */
	msg[0].buf = &reg;                    /* 指向 msg 数据:读取的首地址 */
	msg[0].len = 1;                        /* reg长度 */

	/* msg[1] 读取 数据 */
	msg[1].addr = client->addr;            /* ap3216c地址 */
	msg[1].flags = I2C_M_RD;            /* 标记为读取数据 */
	msg[1].buf = val;                    /* 指向 msg 数据:读取数据缓冲区 */
	msg[1].len = len;                    /* 要读取的数据长度 */

	ret = i2c_transfer(client->adapter, msg/*消息*/, 2/*消息数量*/); // i2c 读写操作
	/* i2c_transfer(): 负值，失败，其他非负值，发送的 msgs 数量 */
	if (ret == 2) {
		ret = 0;
	} else {
		printk("i2c rd failed=%d reg=%06x len=%d\n", ret, reg, len);
		ret = -EREMOTEIO;
	}
	return ret;
}

/*
 * @description	: 读取ap3216c指定寄存器值，读取一个寄存器
 * @param - dev:  ap3216c设备
 * @param - reg:  要读取的寄存器首地址
 * @return 	  :   读取到的寄存器值
 */
static unsigned char ap3216c_read_reg(struct ap3216c_dev *dev, u8 reg)
{
	u8 data = 0;
	/*  AP3216C 好像不支持连续多字节读取，
	 此函数在测试其他 I2C 设备的时候可以实现多给字节连续读取，
	 但是在 AP3216C 上不能连续读取多个字节。不过读取一个字节没有问题的 */
	ap3216c_read_regs(dev, reg, &data, 1); // 读取一个字节
	return data;
#if 0
	struct i2c_client *client = (struct i2c_client *)dev->private_data;/* 私有数据 */
	return i2c_smbus_read_byte_data(client, reg);
#endif
}

/*
 * @description	: 读取AP3216C的数据，读取原始数据，包括ALS,PS和IR, 注意！
 *				: 如果同时打开ALS,IR+PS的话两次数据读取的时间间隔要大于112.5ms
 * @param - ir	: ir数据
 * @param - ps 	: ps数据
 * @param - ps 	: als数据
 * @return 		: 无。
 */
void ap3216c_readdata(struct ap3216c_dev *dev)
{
	unsigned char i = 0;
	unsigned char buf[6];

	/* 循环读取所有传感器数据 */
	for (i = 0; i < 6; i++) {
		buf[i] = ap3216c_read_reg(dev, AP3216C_IRDATALOW + i);
	}

	if (buf[0] & 0X80)    /* IR_OF位为1,则数据无效 */
		dev->ir = 0;
	else                /* 读取IR传感器的数据   		*/
		dev->ir = ((unsigned short) buf[1] << 2) | (buf[0] & 0X03);

	dev->als = ((unsigned short) buf[3] << 8) | buf[2];    /* 读取ALS传感器的数据 */

	if (buf[4] & 0x40)    /* IR_OF 位为 1,则数据无效 */
		dev->ps = 0;
	else {              /* 读取PS传感器的数据 */
		dev->ps = ((unsigned short) (buf[5] & 0X003F) << 4) | (buf[4] & 0X0F);
		dev->ps_mode = buf[4] & 0x80 ? 1 : 0;
	}
}

/*
 * @description		: 从设备读取数据
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t ap3216c_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	unsigned short data[4];
	long err = 0;

	/** 从绑定的 私有数据（ap3216c_dev） 获取数据 */
	struct ap3216c_dev *dev = (struct ap3216c_dev *) filp->private_data;

	ap3216c_readdata(dev);
	data[0] = dev->ir;
	data[1] = dev->als;
	data[2] = dev->ps;
	data[3] = dev->ps_mode;

	/* 	copy_to_user() 将数据从内核空间拷贝到用户空间,一般是在驱动中 chr_drv_read()用
参数1：内核驱动中的一个buffer
参数2：应用空间到一个buffer
参数3：个数   */
	err = copy_to_user(buf, data, sizeof(data));
	return 0;
}

/*
 * @description	: 向ap3216c多个寄存器写入数据:实现连续多字节写操作
 * @param - dev:  ap3216c设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 ap3216c_write_regs(struct ap3216c_dev *dev, u8 reg, u8 *buf, u8 len)
{
	u8 b[256];
	struct i2c_msg msg;
	struct i2c_client *client = (struct i2c_client *) dev->private_data;/* 私有数据 */

	b[0] = reg;                    /* 寄存器首地址 */
	memcpy(&b[1], buf, len);        /* 将要写入的数据拷贝到数组b里面 */

	/* msg为 发送 要写入的寄存器首地址 */
	msg.addr = client->addr;    /* ap3216c地址 */
	msg.flags = 0;                /* 0=写数据 */
	msg.buf = b;                /* 向 msg 数据:要写入的数据缓冲区 */
	msg.len = len + 1;            /* 要写入的数据长度 */

	return i2c_transfer(client->adapter, &msg/*消息*/, 1/*消息数量*/); // i2c 读写操作
	/* i2c_transfer(): 负值，失败，其他非负值，发送的 msgs 数量 */
}

/*
 * @description	: 向ap3216c指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  ap3216c设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */
static void ap3216c_write_reg(struct ap3216c_dev *dev, u8 reg, u8 data)
{
	u8 buf = data;
	ap3216c_write_regs(dev, reg, &buf, 1);
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int ap3216c_open(struct inode *inode, struct file *filp)
{
	/** 绑定 私有数据（ap3216c_dev）  */
	filp->private_data = &ap3216cdev;

	/* 初始化AP3216C */
	ap3216c_write_reg(&ap3216cdev, AP3216C_SYSTEMCONG, SoftReset); /* 软复位 AP3216C */
	mdelay(50);    /* AP3216C 复位最少 10ms 	*/
	ap3216c_write_reg(&ap3216cdev, AP3216C_SYSTEMCONG, MODE_ALS_PS_IR); /* 开启ALS、PS+IR */
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int ap3216c_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* AP3216C 设备操作函数 */
static const struct file_operations ap3216c_ops = {
		.owner = THIS_MODULE,
		.open = ap3216c_open,
		.read = ap3216c_read,
		.release = ap3216c_release,
};

/*
 * @description     : i2c驱动的probe函数，当驱动与
 *                    设备匹配以后此函数就会执行
 * @param - client  : i2c设备
 * @param - id      : i2c设备ID
 * @return          : 0，成功;其他负值,失败
 */
static int ap3216c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (ap3216cdev.major) {/*  定义了设备号 */
		ap3216cdev.devid = MKDEV(ap3216cdev.major, 0);
		register_chrdev_region(ap3216cdev.devid, AP3216C_CNT, AP3216C_NAME);
	} else {/* 没有定义设备号 */
		alloc_chrdev_region(&ap3216cdev.devid, 0, AP3216C_CNT, AP3216C_NAME);/* 申请设备号 */
		ap3216cdev.major = MAJOR(ap3216cdev.devid);/* 获取分配号的主设备号 */
	}

	/* 2、初始化cdev */
	cdev_init(&ap3216cdev.cdev, &ap3216c_ops);

	/* 3、添加一个cdev */
	cdev_add(&ap3216cdev.cdev, ap3216cdev.devid, AP3216C_CNT);

	/* 4、创建类 */
	ap3216cdev.class = class_create(THIS_MODULE, AP3216C_NAME);
	if (IS_ERR(ap3216cdev.class)) {
		return PTR_ERR(ap3216cdev.class);
	}

	/* 5、创建设备 */
	ap3216cdev.device = device_create(ap3216cdev.class, NULL, ap3216cdev.devid, NULL, AP3216C_NAME);
	if (IS_ERR(ap3216cdev.device)) {
		return PTR_ERR(ap3216cdev.device);
	}

	p3216cdeav.private_data = client;
	/*将保存为设备的私有数据，可通过 i2c_get_clientdata 获取数据*/
	// i2c_set_clientdata(client, p3216cdeav.private_data);

	return 0;
}

/*
 * @description     : i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 * @param - client 	: i2c设备
 * @return          : 0，成功;其他负值,失败
 */
static int ap3216c_remove(struct i2c_client *client)
{
	/* 注销字符设备驱动 */
	cdev_del(&ap3216cdev.cdev);/*  删除cdev */
	unregister_chrdev_region(ap3216cdev.devid, AP3216C_CNT);/* 注销设备号 */

	/* 注销掉类和设备 */
	device_destroy(ap3216cdev.class, ap3216cdev.devid);
	class_destroy(ap3216cdev.class);
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct i2c_device_id ap3216c_id[] = {
		{"alientek,ap3216c", 0},
		{}
};

/* 设备树匹配列表 */
static const struct of_device_id ap3216c_of_match[] = {
		{.compatible = "alientek,ap3216c"}, /* 兼容属性 */
		{ /* Sentinel */ }
};

/* i2c驱动结构体 */
static struct i2c_driver ap3216c_driver = {
		.driver = {
				.owner = THIS_MODULE,
				.name = "ap3216c",/* 驱动名字，用于和设备匹配 */
				.of_match_table = ap3216c_of_match, /* 设备树匹配表          */
		},
		.probe = ap3216c_probe,
		.remove = ap3216c_remove,
		.id_table = ap3216c_id,
};

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init

ap3216c_init(void)
{
	int ret = i2c_add_driver(&ap3216c_driver);
	return ret;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit ap3216c_exit(void)
{
	i2c_del_driver(&ap3216c_driver);
}

/* module_i2c_driver(ap3216c_driver) */

module_init(ap3216c_init);
module_exit(ap3216c_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");



