#include <linux/init.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/fs.h>

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <mach/regs-gpio.h>
#include <asm/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/input.h>


#define	MY_KEY_HOME	EXYNOS4_GPX1(1)
#define	MY_KEY_BACK	EXYNOS4_GPX1(2)
#define	MY_KEY_SLEEP	EXYNOS4_GPX3(3)
#define MY_KEY_VOL_U	EXYNOS4_GPX2(1)
#define MY_KEY_VOL_D	EXYNOS4_GPX2(0)

//定义一个输入子系统结构体，它是输入子系统的核心，后面所有工作都是围绕这个结构体还做的！
//第一步：定义之后，向内核申请一个input_dev事件，标准内核函数接口input_allocate_device和input_free_device
//第二步：配置输入子系统的某一类“事件类型”，例如：按键事件，鼠标事件，触摸事件等等
//第三步：配置能产生这类事件中的那些具体操作，例如：按键按键事件中，可以输入键盘a，键盘b等
//第四步：注册输入子系统，标准内核函数接口input_register_device和input_unregister_device
static struct input_dev *buttons_dev;

static irqreturn_t ehome_interrupt(int irq, void *dev_id)
{
	//printk("%s(%d)\n", __FUNCTION__, __LINE__);

	//char *ps = dev_id;
	//printk("%s\n",ps);
	
	//input_event(buttons_dev, EV_KEY, KEY_L, 1);
	//input_sync(buttons_dev);

	//最后一个参数: 0-松开, 非0-按下 
	input_report_key(buttons_dev, KEY_L, 1); 
	//通知上层，本次事件结束
	input_sync(buttons_dev);
	
	input_report_key(buttons_dev, KEY_L, 0); 
	//通知上层，本次事件结束
	input_sync(buttons_dev);
	
	return IRQ_HANDLED;
}


static void __exit keys_input_exit(void)
{
	printk("%s(%d)\n", __FUNCTION__, __LINE__);
	gpio_free(MY_KEY_HOME);
	free_irq(gpio_to_irq(MY_KEY_HOME),(void *)"my_key_home");

//释放申请的input_dev事件以及释放输入子系统
	input_unregister_device(buttons_dev);
	input_free_device(buttons_dev);	
}

static int __init keys_input_init(void)
{
	int ret;
	printk("%s(%d)\n", __FUNCTION__, __LINE__);
	
//第一步：定义之后，向内核申请一个input_dev事件，标准内核函数接口input_allocate_device和input_free_device
	buttons_dev = input_allocate_device();
	
//第二步：配置输入子系统的某一类“事件类型”，例如：按键事件，鼠标事件，触摸事件等等
//在input.h头文件中，定义了10多种事件类型,这里定义按键事件
	set_bit(EV_KEY, buttons_dev->evbit);
	set_bit(EV_REP, buttons_dev->evbit);	//重复扫描

//第三步：配置能产生这类事件中的那些具体操作，例如：按键按键事件中，可以输入键盘a，键盘b等
//这里定义产生l按键值
	set_bit(KEY_L, buttons_dev->keybit);
	
//第四步：注册输入子系统，标准内核函数接口input_register_device和input_unregister_device	
	ret = input_register_device(buttons_dev);
	if(ret <0){
		printk("%s(%d)\n", __FUNCTION__, __LINE__);
		goto exit;
	}
	
	gpio_request(MY_KEY_HOME, "my_key_home");
	s3c_gpio_cfgpin(MY_KEY_HOME, S3C_GPIO_OUTPUT);
	gpio_set_value(MY_KEY_HOME, 0);
	gpio_free(MY_KEY_HOME);
	ret = request_irq(gpio_to_irq(MY_KEY_HOME),ehome_interrupt,IRQ_TYPE_EDGE_FALLING, "my_key_home", (void *)"my_key_home");
	if(ret<0){
		printk("Request IRQ %s failed, %d\n","key_home" , ret);
		goto exit;
	}
	
	return 0;
	
exit:
	return ret;
}

module_init(keys_input_init);
module_exit(keys_input_exit);

MODULE_LICENSE("Dual BSD/GPL");