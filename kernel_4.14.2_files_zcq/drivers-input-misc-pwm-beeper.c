// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2010, Lars-Peter Clausen <lars@metafoo.de>
 *  PWM beeper driver
 */

#include <linux/input.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct pwm_beeper {
	struct input_dev *input;
	struct pwm_device *pwm;
	struct regulator *amplifier;
	struct work_struct work;
	unsigned long period;
	unsigned int bell_frequency;
	bool suspended;
	bool amplifier_on;
};

#define HZ_TO_NANOSECONDS(x) (1000000000UL/(x))

static int pwm_beeper_on(struct pwm_beeper *beeper, unsigned long period)
{
	struct pwm_state state;
	int error;

	pwm_get_state(beeper->pwm, &state);

	state.enabled = true;
	state.period = period;
	pwm_set_relative_duty_cycle(&state, 50, 100);

	error = pwm_apply_state(beeper->pwm, &state);
	if (error)
		return error;

	if (!beeper->amplifier_on) {
		error = regulator_enable(beeper->amplifier);
		if (error) {
			pwm_disable(beeper->pwm);
			return error;
		}

		beeper->amplifier_on = true;
	}

	return 0;
}

static void pwm_beeper_off(struct pwm_beeper *beeper)
{
	if (beeper->amplifier_on) {
		regulator_disable(beeper->amplifier);
		beeper->amplifier_on = false;
	}

	pwm_disable(beeper->pwm);
}

static void pwm_beeper_work(struct work_struct *work)
{
	struct pwm_beeper *beeper = container_of(work, struct pwm_beeper, work);
	unsigned long period = READ_ONCE(beeper->period);

	if (period)
		pwm_beeper_on(beeper, period);
	else
		pwm_beeper_off(beeper);
}

static int pwm_beeper_event(struct input_dev *input,
			    unsigned int type, unsigned int code, int value)
{
	struct pwm_beeper *beeper = input_get_drvdata(input);

	if (type != EV_SND || value < 0)
		return -EINVAL;

	switch (code) {
	case SND_BELL:
		value = value ? beeper->bell_frequency : 0;
		break;
	case SND_TONE:
		break;
	default:
		return -EINVAL;
	}

	if (value == 0)
		beeper->period = 0;
	else
		beeper->period = HZ_TO_NANOSECONDS(value);

	if (!beeper->suspended)
		schedule_work(&beeper->work);

	return 0;
}

static void pwm_beeper_stop(struct pwm_beeper *beeper)
{
	cancel_work_sync(&beeper->work);
	pwm_beeper_off(beeper);
}

static void pwm_beeper_close(struct input_dev *input)
{
	struct pwm_beeper *beeper = input_get_drvdata(input);

	pwm_beeper_stop(beeper);
}

static int pwm_beeper_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pwm_beeper *beeper;
	struct pwm_state state;
	u32 bell_frequency;
	int error;

	beeper = devm_kzalloc(dev, sizeof(*beeper), GFP_KERNEL);
	if (!beeper)
		return -ENOMEM;

	beeper->pwm = devm_pwm_get(dev, NULL);
	if (IS_ERR(beeper->pwm)) {
		error = PTR_ERR(beeper->pwm);
		if (error != -EPROBE_DEFER)
			dev_err(dev, "Failed to request PWM device: %d\n",
				error);
		return error;
	}

	/* Sync up PWM state and ensure it is off. */
	pwm_init_state(beeper->pwm, &state);
	state.enabled = false;
	error = pwm_apply_state(beeper->pwm, &state);
	if (error) {
		dev_err(dev, "failed to apply initial PWM state: %d\n",
			error);
		return error;
	}

	beeper->amplifier = devm_regulator_get(dev, "amp");
	if (IS_ERR(beeper->amplifier)) {
		error = PTR_ERR(beeper->amplifier);
		if (error != -EPROBE_DEFER)
			dev_err(dev, "Failed to get 'amp' regulator: %d\n",
				error);
		return error;
	}

	INIT_WORK(&beeper->work, pwm_beeper_work);

	error = device_property_read_u32(dev, "beeper-hz", &bell_frequency);
	if (error) {
		bell_frequency = 1000;
		dev_dbg(dev,
			"failed to parse 'beeper-hz' property, using default: %uHz\n",
			bell_frequency);
	}

	beeper->bell_frequency = bell_frequency;

	beeper->input = devm_input_allocate_device(dev);
	if (!beeper->input) {
		dev_err(dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	beeper->input->name = "pwm-beeper";
	beeper->input->phys = "pwm/input0";
	beeper->input->id.bustype = BUS_HOST;
	beeper->input->id.vendor = 0x001f;
	beeper->input->id.product = 0x0001;
	beeper->input->id.version = 0x0100;

	input_set_capability(beeper->input, EV_SND, SND_TONE);
	input_set_capability(beeper->input, EV_SND, SND_BELL);

	beeper->input->event = pwm_beeper_event;
	beeper->input->close = pwm_beeper_close;

	input_set_drvdata(beeper->input, beeper);

	error = input_register_device(beeper->input);
	if (error) {
		dev_err(dev, "Failed to register input device: %d\n", error);
		return error;
	}

	platform_set_drvdata(pdev, beeper);

	return 0;
}

static int __maybe_unused pwm_beeper_suspend(struct device *dev)
{
	struct pwm_beeper *beeper = dev_get_drvdata(dev);

	/*
	 * Spinlock is taken here is not to protect write to
	 * beeper->suspended, but to ensure that pwm_beeper_event
	 * does not re-submit work once flag is set.
	 */
	spin_lock_irq(&beeper->input->event_lock);
	beeper->suspended = true;
	spin_unlock_irq(&beeper->input->event_lock);

	pwm_beeper_stop(beeper);

	return 0;
}

static int __maybe_unused pwm_beeper_resume(struct device *dev)
{
	struct pwm_beeper *beeper = dev_get_drvdata(dev);

	spin_lock_irq(&beeper->input->event_lock);
	beeper->suspended = false;
	spin_unlock_irq(&beeper->input->event_lock);

	/* Let worker figure out if we should resume beeping */
	schedule_work(&beeper->work);

	return 0;
}

static SIMPLE_DEV_PM_OPS(pwm_beeper_pm_ops,
			 pwm_beeper_suspend, pwm_beeper_resume);

/** *************** 设备树 *.dts 匹配列表 ********************* **/

#ifdef CONFIG_OF
static const struct of_device_id pwm_beeper_match[] = {
	{ .compatible = "pwm-beeper", },
	{ },
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE(of, pwm_beeper_match);
#endif

/** *************** platform 驱动 结构体 ********************* **/

static struct platform_driver pwm_beeper_driver = {
	.probe	= pwm_beeper_probe, /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.driver = {
		.name	= "pwm-beeper", /* 名字，用于驱动和设备的匹配 */
		.pm	= &pwm_beeper_pm_ops,
		.of_match_table = of_match_ptr(pwm_beeper_match), /* 设备树匹配表 */
	},
};

/********************************************************************/
#if 1
/*驱动模块自动加载、卸载函数*/
module_platform_driver(pwm_beeper_driver);
#else
/*驱动模块加载函数*/
static int __init miscbeep_init(void)
{
	printk(KERN_EMERG "%s()\n", __func__);
	return platform_driver_register(&pwm_beeper_driver); /*向系统注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit miscbeep_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s()\n", __func__);
	platform_driver_unregister(&pwm_beeper_driver); /*向系统注销驱动*/
}

module_init(miscbeep_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(miscbeep_exit); /*卸载驱动时运行的函数， 如 rmmod*/
#endif
/********************************************************************/

MODULE_LICENSE("GPL"); 	/* 声明开源，没有内核版本限制*/
MODULE_AUTHOR("Lars-Peter Clausen <lars@metafoo.de>");/*声明作者*/
MODULE_ALIAS("platform:pwm-beeper");
MODULE_DESCRIPTION("PWM beeper driver");
