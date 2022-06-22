#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>

/**  topeet_relay.dtsi        继电器
/ {
  	relay {
             compatible = "itop4412_relay";
             pinctrl-names = "default";
             relay-gpios = <&gpm4 6 GPIO_ACTIVE_LOW>;
             status = "okay";
         };
}  
-------------------------------------------------------------------
drivers/misc/Kconfig
...
config ITOP4412_RELAY_DEVICE
     bool "Enable RELAY config"
     default y
     help
        Enable RELAY config          */


#define DEVICE_NAME "itop4412_relay_device"
#define DRIVER_NAME "itop4412_relay_driver"


uint32_t RELAY_GPIO = 0;

/********************************************************************/

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int itop4412_relay_open(struct inode *inode, struct file *filp) {
    return 0;
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t itop4412_relay_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
        char str[20];
        memset(str, 0, 20);

	/* copy_from_user(): 将数据从用户空间拷贝到内核空间,一般是在驱动中 chr_drv_write()用
参数1：应用驱动中的一个buffer
参数2：内核空间到一个buffer
参数3：个数
返回值：大于0，表示出错，剩下多少个没有拷贝成功; 等于0，表示正确  */
	if(copy_from_user(str, buf, cnt)) {
		printk("copy_from_user() Error\n");
		return -EINVAL;
	}

	printk("power_write(): %s\r\n", str);
#if 1
        if(!strncmp(str, "1", 1))
            gpio_set_value(RELAY_GPIO, 1);

        else 
            gpio_set_value(RELAY_GPIO, 0);
        
#endif
        return cnt;
}

static int itop4412_relay_close(struct inode *inode, struct file *file) {
    return 0;
}

static long itop4412_relay_ioctl(struct file *filp, unsigned int cmd,
                unsigned long arg)
{
    printk("%s: cmd = %d\n", __FUNCTION__, cmd);
    switch(cmd) {
            case 0:
                    gpio_set_value(RELAY_GPIO, 0);
                    break;
            case 1:
                    gpio_set_value(RELAY_GPIO, 1);
                    break;
            default:
                    return -EINVAL;
    }

    return 0;
}

/********************************************************************/

/* 设备操作结构体 */
static struct file_operations itop4412_relay_ops = {
        .owner					= THIS_MODULE,
        .open                   = itop4412_relay_open,
        .write                  = itop4412_relay_write,
        .release                = itop4412_relay_close,
        .unlocked_ioctl 		= itop4412_relay_ioctl,
};

/* MISC 设备结构体 */
static struct miscdevice itop4412_misc_dev = {
        .minor = MISC_DYNAMIC_MINOR,    // 子设备号
        .name = DEVICE_NAME,            // 设备名字
        .fops = &itop4412_relay_ops,
};

/** *************** platform 驱动 结构体 函数 ********************* **/

static int itop4412_relay_probe (struct platform_device *pdev) {
	int ret;
	
	/* 1、获取设备节点 */
	struct device_node *np = pdev->dev.of_node;/*设备节点*/
	
	/* 2、 获取设备树中的gpio属性，得到BEEP所使用的BEEP编号 */
	RELAY_GPIO = of_get_named_gpio(np, "relay-gpio", 0);
	if (RELAY_GPIO == -EPROBE_DEFER)
			return RELAY_GPIO;
	if (RELAY_GPIO < 0) {
			dev_err(&pdev->dev, "error acquiring relay gpio: %d\n", RELAY_GPIO);
			return RELAY_GPIO;
	}
	
	ret = devm_gpio_request_one(&pdev->dev, RELAY_GPIO, 0, "relay-gpio");
	if(ret) {
			dev_err(&pdev->dev, "error requesting relay gpio: %d\n", ret);
			return ret;
	}
	
	/* 3、设置GPIO5_IO01为输出，并且输出高电平，默认关闭BEEP */
	gpio_direction_output(RELAY_GPIO, 0);
	
	ret = misc_register(&itop4412_misc_dev);
	
	printk(DEVICE_NAME "\tinitialized\n");
	
	return 0;
	
	
}

/*移除驱动*/
static int itop4412_relay_remove (struct platform_device *pdev)
{
	gpio_free(RELAY_GPIO);	/*释放已经被占用的 IO*/
    misc_deregister(&itop4412_misc_dev); /*注销 misc 杂项设备*/

    return 0;
}

/*悬挂（休眠）驱动*/
static int itop4412_relay_suspend (struct platform_device *pdev, pm_message_t state)
{
    printk("relay_ctl suspend:power off!\n");
    return 0;
}

/*驱动恢复后要做什么*/
static int itop4412_relay_resume (struct platform_device *pdev)
{
    printk("relay_ctl resume:power on!\n");
    return 0;
}

/** *************** 设备树 *.dts 匹配列表 ********************* **/

#ifdef CONFIG_OF
static const struct of_device_id relay_of_match[] = {
    { .compatible = "itop4412_relay" }, /* 兼容属性 */
    { /* sentinel */ }
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE(of, relay_of_match);
#endif

/** *************** platform 驱动 结构体 ********************* **/

static struct platform_driver itop4412_relay_driver = {
    .probe = itop4412_relay_probe,      /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
    .remove = itop4412_relay_remove,    /*移除驱动*/
	// .shutdown = hello_shutdown,	/*关闭驱动*/
    .suspend = itop4412_relay_suspend,  /*悬挂（休眠）驱动*/
    .resume = itop4412_relay_resume,    /*驱动恢复后要做什么*/
    .driver = {
            .name = DRIVER_NAME,        /* 驱动名字 */
            .owner = THIS_MODULE,
            .of_match_table = of_match_ptr(relay_of_match), /* 设备树匹配表 */
    },
};

/********************************************************************/

/*驱动模块加载函数*/
static int __init itop4412_relay_dev_init(void){
	return platform_driver_register(&itop4412_relay_driver);	
}

/*驱动模块卸载函数*/
static void __exit itop4412_relay_dev_exit(void) {
	platform_driver_unregister(&itop4412_relay_driver);
}

/********************************************************************/

module_init(itop4412_relay_dev_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(itop4412_relay_dev_exit); /*卸载驱动时运行的函数， 如 rmmod*/

MODULE_LICENSE("GPL"); /*Dual BSD/GPL 声明是开源的，没有内核版本限制*/
MODULE_AUTHOR("zcq TOPEET");			/*声明作者*/
MODULE_DESCRIPTION("itop4412 relay.c driver");
