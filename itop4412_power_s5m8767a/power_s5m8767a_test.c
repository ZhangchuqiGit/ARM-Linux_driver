
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <linux/fs.h>
#include <linux/err.h>

struct regulator *ov_vddaf_cam_regulator = NULL;
struct regulator *ov_vdd5m_cam_regulator = NULL;
struct regulator *ov_vdd18_cam_regulator = NULL;
struct regulator *ov_vdd28_cam_regulator = NULL;

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("iTOPEET_dz");

static int power(int flag)
{
	if(1 == flag){
		regulator_enable(ov_vdd18_cam_regulator);
		udelay(10);
		regulator_enable(ov_vdd28_cam_regulator);
		udelay(10);
		regulator_enable(ov_vdd5m_cam_regulator); //DOVDD  DVDD 1.8v
		udelay(10);
		regulator_enable(ov_vddaf_cam_regulator);         //AVDD 2.8v
		udelay(10);
	}
	else if(0 == flag){
		regulator_disable(ov_vdd18_cam_regulator);
		udelay(10);
		regulator_disable(ov_vdd28_cam_regulator);
		udelay(10);
		regulator_disable(ov_vdd5m_cam_regulator);
		udelay(10);
		regulator_disable(ov_vddaf_cam_regulator);
		udelay(10);
	}
	
	return 0 ;
}

static void power_init(void)
{
	int ret;
	
	ov_vdd18_cam_regulator = regulator_get(NULL, "vdd18_cam");
	if (IS_ERR(ov_vdd18_cam_regulator)) {
		printk("%s: failed to get %s\n", __func__, "vdd18_cam");
		ret = -ENODEV;
		goto err_regulator;
	}

	ov_vdd28_cam_regulator = regulator_get(NULL, "vdda28_2m");
	if (IS_ERR(ov_vdd28_cam_regulator)) {
		printk("%s: failed to get %s\n", __func__, "vdda28_2m");
		ret = -ENODEV;
		goto err_regulator;
	}

	ov_vddaf_cam_regulator = regulator_get(NULL, "vdd28_af");
	if (IS_ERR(ov_vddaf_cam_regulator)) {
		printk("%s: failed to get %s\n", __func__, "vdd28_af");
		ret = -ENODEV;
		goto err_regulator;
	}

	ov_vdd5m_cam_regulator = regulator_get(NULL, "vdd28_cam");
	if (IS_ERR(ov_vdd5m_cam_regulator)) {
		printk("%s: failed to get %s\n", __func__, "vdd28_cam");
		ret = -ENODEV;
		goto err_regulator;
	}

err_regulator:
	regulator_put(ov_vddaf_cam_regulator);
	regulator_put(ov_vdd5m_cam_regulator);
	regulator_put(ov_vdd18_cam_regulator);
	regulator_put(ov_vdd28_cam_regulator);	
}
static int hello_init(void)
{
	power_init();
	
	power(1);
	printk(KERN_EMERG "Hello World enter!\n");
	return 0;
}

static void hello_exit(void)
{
	power(0);
	
	printk(KERN_EMERG "Hello world exit!\n");
}

module_init(hello_init);
module_exit(hello_exit);