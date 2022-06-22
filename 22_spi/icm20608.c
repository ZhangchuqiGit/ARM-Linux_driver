#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/errno.h>
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

#include <linux/gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

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

/*	spi
SDI		数据输入， SPI_2_MOSI, BUF_I2C_SCL6, GPC1_4
SDO		数据输出， SPI_2_MISO, BUF_I2C_SDA6, GPC1_3
SCLK	时钟， SPI_2_CLK, BUF_GPC1_1, GPC1_1
CS		片选, BK_LED, GPC1_2
*/

#include <linux/compat.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/spi/spi.h>
#include "icm20608reg.h"

#define ICM20608_CNT         1           /* 设备号个数*/
#define ICM20608_NAME        "icm20608"  /* 设备名字	*/

struct icm20608_dev {
	dev_t devid;                /* 设备号 	 */
	struct cdev cdev;            /* cdev 	*/
	struct class *class;        /* 类 		*/
	struct device *device;        /* 设备 	 */
	struct device_node *nd;    /* 设备节点 */
	int major;                    /* 主设备号 */

	void *private_data;            /* 私有数据 		*/
	int cs_gpio;                // 所使用的 Gled_switch PIO 编号

	signed int gyro_x_adc;        /* 陀螺仪X轴原始值 	 */
	signed int gyro_y_adc;        /* 陀螺仪Y轴原始值		*/
	signed int gyro_z_adc;        /* 陀螺仪Z轴原始值 		*/
	signed int accel_x_adc;        /* 加速度计X轴原始值 	*/
	signed int accel_y_adc;        /* 加速度计Y轴原始值	*/
	signed int accel_z_adc;        /* 加速度计Z轴原始值 	*/
	signed int temp_adc;        /* 温度原始值 			*/
};

static struct icm20608_dev icm20608dev;



/*
 * @description	: 从icm20608读取多个寄存器数据
 * @param - dev:  icm20608设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int icm20608_read_regs(struct icm20608_dev *dev, u8 reg, void *buf, int len)
{

	int ret = -1;
	unsigned char txdata[1];
	unsigned char * rxdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->private_data;
    
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}

	rxdata = kzalloc(sizeof(char) * len, GFP_KERNEL);	/* 申请内存 */
	if(!rxdata) {
		goto out1;
	}

	/* 一共发送len+1个字节的数据，第一个字节为
	寄存器首地址，一共要读取len个字节长度的数据，*/
	txdata[0] = reg | 0x80;		/* 写数据的时候首寄存器地址bit8要置1 */			
	t->tx_buf = txdata;			/* 要发送的数据 */
    t->rx_buf = rxdata;			/* 要读取的数据 */
	t->len = len+1;				/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
	if(ret) {
		goto out2;
	}
	
    memcpy(buf , rxdata+1, len);  /* 只需要读取的数据 */

out2:
	kfree(rxdata);					/* 释放内存 */
out1:	
	kfree(t);						/* 释放内存 */
	
	return ret;
}
/*
 * @description	: 向icm20608多个寄存器写入数据
 * @param - dev:  icm20608设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 icm20608_write_regs(struct icm20608_dev *dev, u8 reg, u8 *buf, u8 len)
{
	int ret = -1;
	unsigned char *txdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->private_data;
	
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}
	
	txdata = kzalloc(sizeof(char)+len, GFP_KERNEL);
	if(!txdata) {
		goto out1;
	}
	
	/* 一共发送len+1个字节的数据，第一个字节为
	寄存器首地址，len为要写入的寄存器的集合，*/
	*txdata = reg & ~0x80;	/* 写数据的时候首寄存器地址bit8要清零 */
    memcpy(txdata+1, buf, len);	/* 把len个寄存器拷贝到txdata里，等待发送 */
	t->tx_buf = txdata;			/* 要发送的数据 */
	t->len = len+1;				/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
    if(ret) {
        goto out2;
    }
	
out2:
	kfree(txdata);				/* 释放内存 */
out1:
	kfree(t);					/* 释放内存 */
	return ret;
}


/*
 * @description	: 读取icm20608指定寄存器值，读取一个寄存器
 * @param - dev:  icm20608设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static unsigned char icm20608_read_onereg(struct icm20608_dev *dev, u8 reg)
{
	u8 data = 0;
	icm20608_read_regs(dev, reg, &data, 1);
	return data;
}

/*
 * @description	: 向icm20608指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  icm20608设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */	

static void icm20608_write_onereg(struct icm20608_dev *dev, u8 reg, u8 value)
{
	u8 buf = value;
	icm20608_write_regs(dev, reg, &buf, 1);
}

/*
 * @description	: 读取ICM20608的数据，读取原始数据，包括三轴陀螺仪、
 * 				: 三轴加速度计和内部温度。
 * @param - dev	: ICM20608设备
 * @return 		: 无。
 */
void icm20608_readdata(struct icm20608_dev *dev)
{
	unsigned char data[14] = { 0 };
	icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);
	/* 加速度输出 */
	dev->accel_x_adc = (signed short) ((data[0] << 8) | data[1]);
	dev->accel_y_adc = (signed short) ((data[2] << 8) | data[3]);
	dev->accel_z_adc = (signed short) ((data[4] << 8) | data[5]);
	/* 温度输出 */
	dev->temp_adc = (signed short) ((data[6] << 8) | data[7]);
	/* 陀螺仪输出 */
	dev->gyro_x_adc = (signed short) ((data[8] << 8) | data[9]);
	dev->gyro_y_adc = (signed short) ((data[10] << 8) | data[11]);
	dev->gyro_z_adc = (signed short) ((data[12] << 8) | data[13]);
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做pr似有ate_data的成员变量
 * 					  一般在open的时候将private_data似有向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int icm20608_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &icm20608dev; /* 设置私有数据 */
	return 0;
}

/*
 * @description		: 从设备读取数据 
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t icm20608_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	signed int data[7];
	long err = 0;
	struct icm20608_dev *dev = (struct icm20608_dev *) filp->private_data; /* 私有数据 */

	icm20608_readdata(dev);
	/* 加速度输出 */
	data[3] = dev->accel_x_adc;
	data[4] = dev->accel_y_adc;
	data[5] = dev->accel_z_adc;
	/* 温度输出 */
	data[6] = dev->temp_adc;
	/* 陀螺仪输出 */
	data[0] = dev->gyro_x_adc;
	data[1] = dev->gyro_y_adc;
	data[2] = dev->gyro_z_adc;

	/* 	copy_to_user() 将数据从内核空间拷贝到用户空间,一般是在驱动中 chr_drv_read()用
参数1：内核驱动中的一个buffer
参数2：应用空间到一个buffer
参数3：个数   */
	err = copy_to_user(buf, data, sizeof(data));
	return 0;
}



/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int icm20608_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* icm20608 设备操作函数 */
static const struct file_operations icm20608_ops = {
		.owner = THIS_MODULE,
		.open = icm20608_open,
		.read = icm20608_read,
		.release = icm20608_release,
};

/*
 * ICM20608内部寄存器初始化函数 
 * @param  	: 无
 * @return 	: 无
 */
void icm20608_reginit(void)
{
	u8 value = 0;
	
	icm20608_write_onereg(&icm20608dev, ICM20_PWR_MGMT_1, 0x80);
	mdelay(50);
	icm20608_write_onereg(&icm20608dev, ICM20_PWR_MGMT_1, 0x01);
	mdelay(50);

	value = icm20608_read_onereg(&icm20608dev, ICM20_WHO_AM_I);
	printk("ICM20608 ID = %#X\r\n", value);	

	icm20608_write_onereg(&icm20608dev, ICM20_SMPLRT_DIV, 0x00); 	/* 输出速率是内部采样率					*/
	icm20608_write_onereg(&icm20608dev, ICM20_GYRO_CONFIG, 0x18); 	/* 陀螺仪±2000dps量程 				*/
	icm20608_write_onereg(&icm20608dev, ICM20_ACCEL_CONFIG, 0x18); 	/* 加速度计±16G量程 					*/
	icm20608_write_onereg(&icm20608dev, ICM20_CONFIG, 0x04); 		/* 陀螺仪低通滤波BW=20Hz 				*/
	icm20608_write_onereg(&icm20608dev, ICM20_ACCEL_CONFIG2, 0x04); /* 加速度计低通滤波BW=21.2Hz 			*/
	icm20608_write_onereg(&icm20608dev, ICM20_PWR_MGMT_2, 0x00); 	/* 打开加速度计和陀螺仪所有轴 				*/
	icm20608_write_onereg(&icm20608dev, ICM20_LP_MODE_CFG, 0x00); 	/* 关闭低功耗 						*/
	icm20608_write_onereg(&icm20608dev, ICM20_FIFO_EN, 0x00);		/* 关闭FIFO						*/
}

/*
 * @description     : spi驱动的probe函数，当驱动与
 *                    设备匹配以后此函数就会执行
 * @param - client  : spi设备
 * @param - id      : spi设备ID
 *
 */
static int icm20608_probe(struct spi_device *spi)
{
	/* 1、构建设备号 */
	if (icm20608dev.major) {
		icm20608dev.devid = MKDEV(icm20608dev.major, 0);
		register_chrdev_region(icm20608dev.devid, ICM20608_CNT, ICM20608_NAME);
	} else {
		alloc_chrdev_region(&icm20608dev.devid, 0, ICM20608_CNT, ICM20608_NAME);
		icm20608dev.major = MAJOR(icm20608dev.devid);
	}

	/* 2、注册设备 */
	cdev_init(&icm20608dev.cdev, &icm20608_ops);
	cdev_add(&icm20608dev.cdev, icm20608dev.devid, ICM20608_CNT);

	/* 3、创建类 */
	icm20608dev.class = class_create(THIS_MODULE, ICM20608_NAME);
	if (IS_ERR(icm20608dev.class)) {
		return PTR_ERR(icm20608dev.class);
	}

	/* 4、创建设备 */
	icm20608dev.device = device_create(icm20608dev.class, NULL, icm20608dev.devid, NULL, ICM20608_NAME);
	if (IS_ERR(icm20608dev.device)) {
		return PTR_ERR(icm20608dev.device);
	}

	/*初始化spi_device */
	spi->mode = SPI_MODE_0;	/*MODE0，CPOL=0，CPHA=0*/
	spi_setup(spi);
	icm20608dev.private_data = spi; /* 设置私有数据 */

	/* 初始化ICM20608内部寄存器 */
	icm20608_reginit();		
	return 0;
}

/*
 * @description     : spi驱动的remove函数，移除spi驱动的时候此函数会执行
 * @param - client 	: spi设备
 * @return          : 0，成功;其他负值,失败
 */
static int icm20608_remove(struct spi_device *spi)
{
	/* 删除设备 */
	cdev_del(&icm20608dev.cdev);
	unregister_chrdev_region(icm20608dev.devid, ICM20608_CNT);

	/* 注销掉类和设备 */
	device_destroy(icm20608dev.class, icm20608dev.devid);
	class_destroy(icm20608dev.class);
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct spi_device_id icm20608_id[] = {
		{"alientek,icm20608", 0},
		{}
};

/* 设备树匹配列表 */
static const struct of_device_id icm20608_of_match[] = {
		{.compatible = "alientek,icm20608"},
		{ /* Sentinel */ }
};

/* SPI驱动结构体 */
static struct spi_driver icm20608_driver = {
		.probe = icm20608_probe,
		.remove = icm20608_remove,
		.driver = {
				.owner = THIS_MODULE,
				.name = "icm20608",/* 驱动名字，用于和设备匹配 */
				.of_match_table = icm20608_of_match, /* 设备树匹配表*/
		},
		.id_table = icm20608_id,
};
		   
/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init icm20608_init(void)
{
	return spi_register_driver(&icm20608_driver);
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit icm20608_exit(void)
{
	spi_unregister_driver(&icm20608_driver);
}

module_init(icm20608_init);
module_exit(icm20608_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
