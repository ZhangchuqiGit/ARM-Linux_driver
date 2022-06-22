/*
 *  exynos_adc.c - Support for ADC in EXYNOS SoCs
 *
 *  8 ~ 10 channel, 10/12-bit ADC
 *
 *  Copyright (C) 2013 Naveen Krishna Chatradhi <ch.naveen@samsung.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/input.h>

#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include <linux/platform_data/touchscreen-s3c2410.h>

	/** ADC使用的是SOC自带的功能，一般SOC厂家已经把相应的驱动代码写好，
	我们只需要在设备树中使能该功能则可。在进行ADC读操作时，
	只需要了解对IIO子系统的使用操作，即可完成ADC的读取

	编译并烧写内核，启动后即可在终端下运行以下命令来读取 ADC0 的值
	数据采集的过程中，旋转电位器的旋钮，改变电位器的电阻分压，就会改变转换后的结果。
	cat /sys/devices/platform/126c0000.adc/iio:device0/in_voltage0_raw
	cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw
	/dev/iio:device0

	itop4412 一共有 4 路 ADC 接口
	* 网络标号是 XadcAIN0~XadcAIN3 
	* 开发板自带的 ADC 电路， ADC 接的是滑动变阻器
	Power Supply Voltage: 1.8V (Typ.), 1.0V (Typ., Digital I/O Interface)
	Analog Input Range: 0 ~ 1.8V 
	Exynos 4412 has two ADC blocks, ADC_CFG[16] setting :
		General ADC : 0x126C_0000
		MTCADC_ISP :  0x1215_0000  
	sec_exynos4412_users manual_ver.1.00.00.pdf  
	57 ADC 57.7	p2770

	/proc/device-tree/adc@126C0000 */

#define ADC_V1_CON(x)		((x) + 0x00)
#define ADC_V1_TSC(x)		((x) + 0x04)
#define ADC_V1_DLY(x)		((x) + 0x08)

	/* ADC Conversion Data Register 
	DATA [11:0] Data value: 0x0 ~ 0xFFF */
#define ADC_V1_DATX(x)		((x) + 0x0C)	/* 0x0C-CDEF */
#define ADC_V1_DATY(x)		((x) + 0x10)	/* 0x10-1234 */
#define ADC_V1_UPDN(x)		((x) + 0x14)	/* 0x14-4567 */

#define ADC_V1_INTCLR(x)	((x) + 0x18) /*INT_ADCn interrupt clear.*/
#define ADC_V1_MUX(x)		((x) + 0x1c) /*	ADCMUX[3:0] 指定模拟输入通道选择
0000 = AIN 0;	0001 = AIN 1;	0010 = AIN 2;	0011 = AIN 3  */

#define ADC_V1_CLRINTPNDNUP(x)	((x) + 0x20)

/* S3C2410 ADC registers definitions */
#define ADC_S3C2410_MUX(x)	((x) + 0x18)

/* Future ADC_V2 registers definitions */
#define ADC_V2_CON1(x)		((x) + 0x00)
#define ADC_V2_CON2(x)		((x) + 0x04)
#define ADC_V2_STAT(x)		((x) + 0x08)
#define ADC_V2_INT_EN(x)	((x) + 0x10)
#define ADC_V2_INT_ST(x)	((x) + 0x14)
#define ADC_V2_VER(x)		((x) + 0x20)

/* Bit definitions for ADC_V1_CON */
#define ADC_V1_CON_RES		(1u << 16)				/*ADCCON[16] 12位ADC */
#define ADC_V1_CON_PRSCEN	(1u << 14)				/*ADCCON[14] 开启/关闭 预分频 */
#define ADC_V1_CON_PRSCLV(x)	(((x) & 0xFF) << 6)	/*ADCCON[6] 设置预分频值: 19 ~ 255 */
#define ADC_V1_CON_STANDBY	(1u << 2)				/*ADCCON[2] 开启/关闭 */

/* Bit definitions for S3C2410 ADC */
#define ADC_S3C2410_CON_SELMUX(x) (((x) & 7) << 3)
#define ADC_S3C2410_DATX_MASK	0x3FF
#define ADC_S3C2416_CON_RES_SEL	(1u << 3)

/* touch screen always uses channel 0 */
#define ADC_S3C2410_MUX_TS	0

/* ADCTSC Register Bits */
#define ADC_S3C2443_TSC_UD_SEN		(1u << 8)
#define ADC_S3C2410_TSC_YM_SEN		(1u << 7)
#define ADC_S3C2410_TSC_YP_SEN		(1u << 6)
#define ADC_S3C2410_TSC_XM_SEN		(1u << 5)
#define ADC_S3C2410_TSC_XP_SEN		(1u << 4)
#define ADC_S3C2410_TSC_PULL_UP_DISABLE	(1u << 3)
#define ADC_S3C2410_TSC_AUTO_PST	(1u << 2)
#define ADC_S3C2410_TSC_XY_PST(x)	(((x) & 0x3) << 0)

#define ADC_TSC_WAIT4INT (ADC_S3C2410_TSC_YM_SEN | \
			 ADC_S3C2410_TSC_YP_SEN | \
			 ADC_S3C2410_TSC_XP_SEN | \
			 ADC_S3C2410_TSC_XY_PST(3))

#define ADC_TSC_AUTOPST	(ADC_S3C2410_TSC_YM_SEN | \
			 ADC_S3C2410_TSC_YP_SEN | \
			 ADC_S3C2410_TSC_XP_SEN | \
			 ADC_S3C2410_TSC_AUTO_PST | \
			 ADC_S3C2410_TSC_XY_PST(0))

/* Bit definitions for ADC_V2 */
#define ADC_V2_CON1_SOFT_RESET	(1u << 2)

#define ADC_V2_CON2_OSEL	(1u << 10)
#define ADC_V2_CON2_ESEL	(1u << 9)
#define ADC_V2_CON2_HIGHF	(1u << 8)
#define ADC_V2_CON2_C_TIME(x)	(((x) & 7) << 4)
#define ADC_V2_CON2_ACH_SEL(x)	(((x) & 0xF) << 0)
#define ADC_V2_CON2_ACH_MASK	0xF

#define MAX_ADC_V2_CHANNELS		10
#define MAX_ADC_V1_CHANNELS		8
#define MAX_EXYNOS3250_ADC_CHANNELS	2
#define MAX_EXYNOS4412_ADC_CHANNELS	4
#define MAX_S5PV210_ADC_CHANNELS	10

/* Bit definitions common for ADC_V1 and ADC_V2 */
#define ADC_CON_EN_START	(1u << 0)
#define ADC_CON_EN_START_MASK	(0x3 << 0)
#define ADC_DATX_PRESSED	(1u << 15)
#define ADC_DATX_MASK		0xFFF	/* ADC DATA [11:0] value: 0x0 ~ 0xFFF */
#define ADC_DATY_MASK		0xFFF	/* ADC DATA [11:0] value: 0x0 ~ 0xFFF */

#define EXYNOS_ADC_TIMEOUT	(msecs_to_jiffies(100))

/* Power Management Unit : ADC_PHY_CONTROL 0x1002_0718 */
#define EXYNOS_ADCV1_PHY_OFFSET	0x0718
#define EXYNOS_ADCV2_PHY_OFFSET	0x0720

struct exynos_adc {
	struct exynos_adc_data	*data;
	struct device		*dev;
	struct input_dev	*input;
	void __iomem		*regs;		/*获取到 DTS 的 reg 的信息*/
	struct regmap		*pmu_map;	/*samsung,syscon-phandle = <&pmu_system_controller>;*/
	struct clk			*clk;		/*获取 DTS 中 clock 的信息*/
	struct clk			*sclk;
	unsigned int		irq;		/* 获取 DTS 中 interrupts 的信息(中断号) */
	unsigned int		tsirq;
	unsigned int		delay;
	struct regulator	*vdd;

	struct completion	completion;

	u32			value;
	unsigned int            version;

	bool			read_ts;
	u32			ts_x;
	u32			ts_y;
};

struct exynos_adc_data {
	int num_channels;
	bool needs_sclk;
	bool 	needs_adc_phy;	/* Power Management Unit */
	int 	phy_offset;		/* Power Management Unit : ADC_PHY_CONTROL 0x1002_0718 */
	u32 mask;

	void (*init_hw)		(struct exynos_adc *info);	/*ADC硬件初始化*/
	void (*exit_hw)		(struct exynos_adc *info);
	void (*clear_irq)	(struct exynos_adc *info);

	/*选择通道+启动转换*/
	void (*start_conv)	(struct exynos_adc *info, unsigned long addr);
};

/********************************************************************/

static int exynos_adc_prepare_clk(struct exynos_adc *info) {
	int ret = clk_prepare(info->clk);
	if (ret) {
		dev_err(info->dev, "failed preparing adc clock: %d\n", ret);
		return ret;
	}
	if (info->data->needs_sclk) {
		ret = clk_prepare(info->sclk);
		if (ret) {
			clk_unprepare(info->clk);
			dev_err(info->dev,
				"failed preparing sclk_adc clock: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static void exynos_adc_unprepare_clk(struct exynos_adc *info) {
	if (info->data->needs_sclk) clk_unprepare(info->sclk);
	clk_unprepare(info->clk);
}

static int exynos_adc_enable_clk(struct exynos_adc *info) {
	int ret = clk_enable(info->clk);
	if (ret) {
		dev_err(info->dev, "failed enabling adc clock: %d\n", ret);
		return ret;
	}
	if (info->data->needs_sclk) {
		ret = clk_enable(info->sclk);
		if (ret) {
			clk_disable(info->clk);
			dev_err(info->dev,
				"failed enabling sclk_adc clock: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static void exynos_adc_disable_clk(struct exynos_adc *info) {
	if (info->data->needs_sclk) clk_disable(info->sclk);
	clk_disable(info->clk);
}

/****************************************************************************/

/*ADC硬件初始化*/
static void exynos_adc_v1_init_hw(struct exynos_adc *info)
{
	u32 con1;

	if (info->data->needs_adc_phy)
		regmap_write(info->pmu_map, info->data->phy_offset, 1);

	/* set default prescaler values and Enable prescaler */
	con1 =  ADC_V1_CON_PRSCLV(49) | ADC_V1_CON_PRSCEN;

	/* Enable 12-bit ADC resolution */
	con1 |= ADC_V1_CON_RES;		
	writel(con1, ADC_V1_CON(info->regs));

	/* set touchscreen delay */
	writel(info->delay, ADC_V1_DLY(info->regs));
}

static void exynos_adc_v1_exit_hw(struct exynos_adc *info)
{
	u32 con;
	if (info->data->needs_adc_phy)
		regmap_write(info->pmu_map, info->data->phy_offset, 0);
	con = readl(ADC_V1_CON(info->regs));
	con |= ADC_V1_CON_STANDBY;
	writel(con, ADC_V1_CON(info->regs));
}

static void exynos_adc_v1_clear_irq(struct exynos_adc *info)
{
	writel(1, ADC_V1_INTCLR(info->regs));
}

/*选择通道+启动转换*/
static void exynos_adc_v1_start_conv(struct exynos_adc *info,
				     unsigned long addr)
{
	u32 con1;
	writel(addr, ADC_V1_MUX(info->regs));	/*选择通道*/
	con1 = readl(ADC_V1_CON(info->regs));	/*启动转换*/
	writel(con1 | ADC_CON_EN_START, ADC_V1_CON(info->regs));
}

/* Exynos4212 and 4412 is like ADCv1 but with four channels only */
static const struct exynos_adc_data exynos4412_adc_data = {
	.num_channels	= MAX_EXYNOS4412_ADC_CHANNELS,
	.mask		= ADC_DATX_MASK,	/* 12 bit ADC resolution */

	.needs_adc_phy	= true,
	.phy_offset	= EXYNOS_ADCV1_PHY_OFFSET,

	.init_hw	= exynos_adc_v1_init_hw,	/*ADC硬件初始化*/
	.exit_hw	= exynos_adc_v1_exit_hw,
	.clear_irq	= exynos_adc_v1_clear_irq,
	.start_conv	= exynos_adc_v1_start_conv,	/*选择通道+启动转换*/
};

static const struct exynos_adc_data exynos_adc_v1_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_DATX_MASK,	/* 12 bit ADC resolution */

	.needs_adc_phy	= true,
	.phy_offset	= EXYNOS_ADCV1_PHY_OFFSET,

	.init_hw	= exynos_adc_v1_init_hw,	/*ADC硬件初始化*/
	.exit_hw	= exynos_adc_v1_exit_hw,
	.clear_irq	= exynos_adc_v1_clear_irq,
	.start_conv	= exynos_adc_v1_start_conv,	/*选择通道+启动转换*/
};

static const struct exynos_adc_data exynos_adc_s5pv210_data = {
	.num_channels	= MAX_S5PV210_ADC_CHANNELS,
	.mask		= ADC_DATX_MASK,	/* 12 bit ADC resolution */

	.init_hw	= exynos_adc_v1_init_hw,
	.exit_hw	= exynos_adc_v1_exit_hw,
	.clear_irq	= exynos_adc_v1_clear_irq,
	.start_conv	= exynos_adc_v1_start_conv,
};

/****************************************************************************/

static void exynos_adc_s3c2416_start_conv(struct exynos_adc *info,
					  unsigned long addr)
{
	u32 con1;

	/* Enable 12 bit ADC resolution */
	con1 = readl(ADC_V1_CON(info->regs));
	con1 |= ADC_S3C2416_CON_RES_SEL;
	writel(con1, ADC_V1_CON(info->regs));

	/* Select channel for S3C2416 */
	writel(addr, ADC_S3C2410_MUX(info->regs));

	con1 = readl(ADC_V1_CON(info->regs));
	writel(con1 | ADC_CON_EN_START, ADC_V1_CON(info->regs));
}

static struct exynos_adc_data const exynos_adc_s3c2416_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_DATX_MASK,	/* 12 bit ADC resolution */

	.init_hw	= exynos_adc_v1_init_hw,	/*ADC硬件初始化*/
	.exit_hw	= exynos_adc_v1_exit_hw,
	.start_conv	= exynos_adc_s3c2416_start_conv,	/*选择通道+启动转换*/
};

static void exynos_adc_s3c2443_start_conv(struct exynos_adc *info,
					  unsigned long addr)
{
	u32 con1;

	/* Select channel for S3C2433 */
	writel(addr, ADC_S3C2410_MUX(info->regs));

	con1 = readl(ADC_V1_CON(info->regs));
	writel(con1 | ADC_CON_EN_START, ADC_V1_CON(info->regs));
}

static struct exynos_adc_data const exynos_adc_s3c2443_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_S3C2410_DATX_MASK, /* 10 bit ADC resolution */

	.init_hw	= exynos_adc_v1_init_hw,	/*ADC硬件初始化*/
	.exit_hw	= exynos_adc_v1_exit_hw,
	.start_conv	= exynos_adc_s3c2443_start_conv,	/*选择通道+启动转换*/
};

static void exynos_adc_s3c64xx_start_conv(struct exynos_adc *info,
					  unsigned long addr)
{
	u32 con1;

	con1 = readl(ADC_V1_CON(info->regs));
	con1 &= ~ADC_S3C2410_CON_SELMUX(0x7);
	con1 |= ADC_S3C2410_CON_SELMUX(addr);
	writel(con1 | ADC_CON_EN_START, ADC_V1_CON(info->regs));
}

static struct exynos_adc_data const exynos_adc_s3c24xx_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_S3C2410_DATX_MASK, /* 10 bit ADC resolution */

	.init_hw	= exynos_adc_v1_init_hw,	/*ADC硬件初始化*/
	.exit_hw	= exynos_adc_v1_exit_hw,
	.start_conv	= exynos_adc_s3c64xx_start_conv,	/*选择通道+启动转换*/
};

static struct exynos_adc_data const exynos_adc_s3c64xx_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_DATX_MASK,	/* 12 bit ADC resolution */

	.init_hw	= exynos_adc_v1_init_hw,	/*ADC硬件初始化*/
	.exit_hw	= exynos_adc_v1_exit_hw,
	.clear_irq	= exynos_adc_v1_clear_irq,
	.start_conv	= exynos_adc_s3c64xx_start_conv,	/*选择通道+启动转换*/
};

/********************************************************************/

/*ADC硬件初始化*/
static void exynos_adc_v2_init_hw(struct exynos_adc *info)
{
	u32 con1, con2;

	if (info->data->needs_adc_phy)
		regmap_write(info->pmu_map, info->data->phy_offset, 1);

	con1 = ADC_V2_CON1_SOFT_RESET;
	writel(con1, ADC_V2_CON1(info->regs));

	con2 = ADC_V2_CON2_OSEL | ADC_V2_CON2_ESEL |
		ADC_V2_CON2_HIGHF | ADC_V2_CON2_C_TIME(0);
	writel(con2, ADC_V2_CON2(info->regs));

	/* Enable interrupts */
	writel(1, ADC_V2_INT_EN(info->regs));
}

static void exynos_adc_v2_exit_hw(struct exynos_adc *info)
{
	u32 con;

	if (info->data->needs_adc_phy)
		regmap_write(info->pmu_map, info->data->phy_offset, 0);

	con = readl(ADC_V2_CON1(info->regs));
	con &= ~ADC_CON_EN_START;
	writel(con, ADC_V2_CON1(info->regs));
}

static void exynos_adc_v2_clear_irq(struct exynos_adc *info)
{
	writel(1, ADC_V2_INT_ST(info->regs));
}

static void exynos_adc_v2_start_conv(struct exynos_adc *info,
				     unsigned long addr)
{
	u32 con1, con2;

	con2 = readl(ADC_V2_CON2(info->regs));
	con2 &= ~ADC_V2_CON2_ACH_MASK;
	con2 |= ADC_V2_CON2_ACH_SEL(addr);
	writel(con2, ADC_V2_CON2(info->regs));

	con1 = readl(ADC_V2_CON1(info->regs));
	writel(con1 | ADC_CON_EN_START, ADC_V2_CON1(info->regs));
}

static const struct exynos_adc_data exynos_adc_v2_data = {
	.num_channels	= MAX_ADC_V2_CHANNELS,
	.mask		= ADC_DATX_MASK, /* 12 bit ADC resolution */
	.needs_adc_phy	= true,
	.phy_offset	= EXYNOS_ADCV2_PHY_OFFSET,

	.init_hw	= exynos_adc_v2_init_hw,	/*ADC硬件初始化*/
	.exit_hw	= exynos_adc_v2_exit_hw,
	.clear_irq	= exynos_adc_v2_clear_irq,
	.start_conv	= exynos_adc_v2_start_conv,	/*选择通道+启动转换*/
};

static const struct exynos_adc_data exynos3250_adc_data = {
	.num_channels	= MAX_EXYNOS3250_ADC_CHANNELS,
	.mask		= ADC_DATX_MASK, /* 12 bit ADC resolution */
	.needs_sclk	= true,
	.needs_adc_phy	= true,
	.phy_offset	= EXYNOS_ADCV1_PHY_OFFSET,

	.init_hw	= exynos_adc_v2_init_hw,	/*ADC硬件初始化*/
	.exit_hw	= exynos_adc_v2_exit_hw,
	.clear_irq	= exynos_adc_v2_clear_irq,
	.start_conv	= exynos_adc_v2_start_conv,	/*选择通道+启动转换*/
};

/********************************************************************/

/*ADC硬件初始化*/
static void exynos_adc_exynos7_init_hw(struct exynos_adc *info)
{
	u32 con1, con2;

	if (info->data->needs_adc_phy)
		regmap_write(info->pmu_map, info->data->phy_offset, 1);

	con1 = ADC_V2_CON1_SOFT_RESET;
	writel(con1, ADC_V2_CON1(info->regs));

	con2 = readl(ADC_V2_CON2(info->regs));
	con2 &= ~ADC_V2_CON2_C_TIME(7);
	con2 |= ADC_V2_CON2_C_TIME(0);
	writel(con2, ADC_V2_CON2(info->regs));

	/* Enable interrupts */
	writel(1, ADC_V2_INT_EN(info->regs));
}

static const struct exynos_adc_data exynos7_adc_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_DATX_MASK, /* 12 bit ADC resolution */

	.init_hw	= exynos_adc_exynos7_init_hw,	/*ADC硬件初始化*/
	.exit_hw	= exynos_adc_v2_exit_hw,
	.clear_irq	= exynos_adc_v2_clear_irq,
	.start_conv	= exynos_adc_v2_start_conv,	/*选择通道+启动转换*/
};

/** *************** 设备树 *.dts 匹配列表 ********************* **/

static const struct of_device_id exynos_adc_match[] = {
	{
		.compatible = "samsung,s3c2410-adc",
		.data = &exynos_adc_s3c24xx_data,
	}, {
		.compatible = "samsung,s3c2416-adc",
		.data = &exynos_adc_s3c2416_data,
	}, {
		.compatible = "samsung,s3c2440-adc",
		.data = &exynos_adc_s3c24xx_data,
	}, {
		.compatible = "samsung,s3c2443-adc",
		.data = &exynos_adc_s3c2443_data,
	}, {
		.compatible = "samsung,s3c6410-adc",
		.data = &exynos_adc_s3c64xx_data,
	}, {
		.compatible = "samsung,s5pv210-adc",
		.data = &exynos_adc_s5pv210_data,
	}, {
		.compatible = "samsung,exynos4412-adc",		 	/* 兼容属性 */
		.data = &exynos4412_adc_data,
	}, {
		.compatible = "samsung,exynos-adc-v1",
		.data = &exynos_adc_v1_data,
	}, {
		.compatible = "samsung,exynos-adc-v2",
		.data = &exynos_adc_v2_data,
	}, {
		.compatible = "samsung,exynos3250-adc",
		.data = &exynos3250_adc_data,
	}, {
		.compatible = "samsung,exynos7-adc",
		.data = &exynos7_adc_data,
	},
	{},
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE(of, exynos_adc_match);

/********************************************************************/

static struct exynos_adc_data *exynos_adc_get_data(struct platform_device *pdev)
{
	/*匹配后的设备树节点*/
	const struct of_device_id *match = of_match_node(exynos_adc_match, pdev->dev.of_node);
	return (struct exynos_adc_data *)match->data;
}

/********************************************************************/

/*读设备内部数据*/
static int exynos_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val,
				int *val2,
				long mask)
{
	struct exynos_adc *info = iio_priv(indio_dev);
	unsigned long timeout;
	int ret;

	if (mask != IIO_CHAN_INFO_RAW) return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	reinit_completion(&info->completion);

	/* Select the channel to be used and Trigger conversion */
	if (info->data->start_conv)
		info->data->start_conv(info, chan->address);	/*选择通道+启动转换*/

	timeout = wait_for_completion_timeout(&info->completion,
					      EXYNOS_ADC_TIMEOUT);
	if (timeout == 0) {
		dev_warn(&indio_dev->dev, "Conversion timed out! Resetting\n");
		if (info->data->init_hw) info->data->init_hw(info);	/*ADC硬件初始化*/
		ret = -ETIMEDOUT;
	} else {
		*val = info->value;	/*读取转换数据*/
		*val2 = 0;
		ret = IIO_VAL_INT;
	}

	mutex_unlock(&indio_dev->mlock);

	return ret;
}

static int exynos_read_s3c64xx_ts(struct iio_dev *indio_dev, int *x, int *y)
{
	struct exynos_adc *info = iio_priv(indio_dev);
	unsigned long timeout;
	int ret;

	mutex_lock(&indio_dev->mlock);
	info->read_ts = true;

	reinit_completion(&info->completion);

	writel(ADC_S3C2410_TSC_PULL_UP_DISABLE | ADC_TSC_AUTOPST,
	       ADC_V1_TSC(info->regs));

	/* Select the ts channel to be used and Trigger conversion */
	info->data->start_conv(info, ADC_S3C2410_MUX_TS);	/*选择通道+启动转换*/

	timeout = wait_for_completion_timeout(&info->completion,
					      EXYNOS_ADC_TIMEOUT);
	if (timeout == 0) {
		dev_warn(&indio_dev->dev, "Conversion timed out! Resetting\n");
		if (info->data->init_hw) info->data->init_hw(info);/*ADC硬件初始化*/
		ret = -ETIMEDOUT;
	} else {
		*x = info->ts_x;
		*y = info->ts_y;
		ret = 0;
	}

	info->read_ts = false;
	mutex_unlock(&indio_dev->mlock);

	return ret;
}

static irqreturn_t exynos_adc_isr(int irq, void *dev_id)
{
	struct exynos_adc *info = dev_id;
	u32 mask = info->data->mask;

	/* Read value */
	if (info->read_ts) {
		info->ts_x = readl(ADC_V1_DATX(info->regs));
		info->ts_y = readl(ADC_V1_DATY(info->regs));
		writel(ADC_TSC_WAIT4INT | ADC_S3C2443_TSC_UD_SEN, ADC_V1_TSC(info->regs));
	} else {
		info->value = readl(ADC_V1_DATX(info->regs)) & mask;
	}

	/* clear irq */
	if (info->data->clear_irq)
		info->data->clear_irq(info);

	complete(&info->completion);

	return IRQ_HANDLED;
}

/*
 * Here we (ab)use a threaded interrupt handler to stay running
 * for as long as the touchscreen remains pressed, we report
 * a new event with the latest data and then sleep until the
 * next timer tick. This mirrors the behavior of the old
 * driver, with much less code.
 */
static irqreturn_t exynos_ts_isr(int irq, void *dev_id)
{
	struct exynos_adc *info = dev_id;
	struct iio_dev *dev = dev_get_drvdata(info->dev);
	u32 x, y;
	bool pressed;
	int ret;

	while (info->input->users) {
		ret = exynos_read_s3c64xx_ts(dev, &x, &y);
		if (ret == -ETIMEDOUT)
			break;

		pressed = x & y & ADC_DATX_PRESSED;
		if (!pressed) {
			input_report_key(info->input, BTN_TOUCH, 0);
			input_sync(info->input);
			break;
		}

		input_report_abs(info->input, ABS_X, x & ADC_DATX_MASK);
		input_report_abs(info->input, ABS_Y, y & ADC_DATY_MASK);
		input_report_key(info->input, BTN_TOUCH, 1);
		input_sync(info->input);

		usleep_range(1000, 1100);
	};

	writel(0, ADC_V1_CLRINTPNDNUP(info->regs));

	return IRQ_HANDLED;
}

/* 函数读取或写入设备的寄存器值 */
static int exynos_adc_reg_access(struct iio_dev *indio_dev,
			      unsigned reg, unsigned writeval,
			      unsigned *readval)
{
	struct exynos_adc *info = iio_priv(indio_dev);
	if (readval == NULL) return -EINVAL;
	*readval = readl(info->regs + reg);
	return 0;
}

/********************************************************************/

/* iio_info{} 这个结构体里面有很多函数，需要驱动开发人员编写，非常重要！
我们从用户空间读取 IIO 设备内部数据，最终调用的就是 iio_info 里面的函数 */
static const struct iio_info exynos_adc_iio_info = {
	.read_raw = &exynos_read_raw,	/*读设备内部数据*/
	.debugfs_reg_access = &exynos_adc_reg_access,	/* 函数读取或写入设备的寄存器值 */
};

/* 可用的通道列表 */
#define ADC_CHANNEL(_index, _id) {			\
	.type = IIO_VOLTAGE,				\
	.indexed = 1,					\
	.channel = _index,				\
	.address = _index,				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),	\
	.datasheet_name = _id,				\
}

/* 通道数据/信息 */
static const struct iio_chan_spec exynos_adc_iio_channels[] = {
	ADC_CHANNEL(0, "adc0"),	/** Xadc1AIN_0 - Xadc1AIN_3 **/
	ADC_CHANNEL(1, "adc1"),
	ADC_CHANNEL(2, "adc2"),
	ADC_CHANNEL(3, "adc3"),	/** exynos4412 adc Analog input Channel 0-3 **/
	ADC_CHANNEL(4, "adc4"),
	ADC_CHANNEL(5, "adc5"),
	ADC_CHANNEL(6, "adc6"),
	ADC_CHANNEL(7, "adc7"),
	ADC_CHANNEL(8, "adc8"),
	ADC_CHANNEL(9, "adc9"),
};

/********************************************************************/

static int exynos_adc_remove_devices(struct device *dev, void *c)
{
	struct platform_device *pdev = to_platform_device(dev);
	platform_device_unregister(pdev);
	return 0;
}

static int exynos_adc_ts_open(struct input_dev *dev)
{
	struct exynos_adc *info = input_get_drvdata(dev);

	enable_irq(info->tsirq);

	return 0;
}

static void exynos_adc_ts_close(struct input_dev *dev)
{
	struct exynos_adc *info = input_get_drvdata(dev);

	disable_irq(info->tsirq);
}

/*ADC硬件初始化*/
static int exynos_adc_ts_init(struct exynos_adc *info)
{
	int ret;

	if (info->tsirq <= 0)
		return -ENODEV;

	info->input = input_allocate_device();
	if (!info->input)
		return -ENOMEM;

	info->input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	info->input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(info->input, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(info->input, ABS_Y, 0, 0x3FF, 0, 0);

	info->input->name = "S3C24xx TouchScreen";
	info->input->id.bustype = BUS_HOST;
	info->input->open = exynos_adc_ts_open;
	info->input->close = exynos_adc_ts_close;

	input_set_drvdata(info->input, info);

	ret = input_register_device(info->input);
	if (ret) {
		input_free_device(info->input);
		return ret;
	}

	disable_irq(info->tsirq);
	ret = request_threaded_irq(info->tsirq, NULL, exynos_ts_isr,
				   IRQF_ONESHOT, "touchscreen", info);
	if (ret)
		input_unregister_device(info->input);

	return ret;
}

/** *************** platform 驱动 结构体 函数 ********************* **/

/*匹配设备时加载驱动, 当驱动与设备匹配以后此函数就会执行*/
static int exynos_adc_probe(struct platform_device *pdev)
{
	struct exynos_adc *info = NULL;
	struct device_node *np = pdev->dev.of_node;	/* 获取设备节点 */
	struct s3c2410_ts_mach_info *pdata = dev_get_platdata(&pdev->dev);
	struct iio_dev *indio_dev = NULL;
	struct resource	*mem = NULL;
	bool has_ts = false;
	int ret = -ENODEV;
	int irq;

	/* 1、动态申请iio设备内存 */
	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct exynos_adc));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}

	/* 2、获取 自定义结构体变量首地址
	使用 iio_priv 函数从 iio_dev 中提取出 sizeof_priv 大小的 私有数据
	由 ...iio_device_alloc(..., int sizeof_priv) 分配 */
	info = iio_priv(indio_dev);

	info->data = exynos_adc_get_data(pdev);
	if (!info->data) {
		dev_err(&pdev->dev, "failed getting exynos_adc_data\n");
		return -EINVAL;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM/*获取DTS中reg的信息*/, 0);
	//将获取到 DTS 的 reg 的信息，进行 ioremap，为后续操作做准备
	info->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(info->regs)) return PTR_ERR(info->regs);

	if (info->data->needs_adc_phy) {
		info->pmu_map = syscon_regmap_lookup_by_phandle(
					pdev->dev.of_node,
					"samsung,syscon-phandle");
		if (IS_ERR(info->pmu_map)) {
			dev_err(&pdev->dev, "syscon regmap lookup failed.\n");
			return PTR_ERR(info->pmu_map);
		}
	}

	irq = platform_get_irq(pdev, 0);/* 获取 DTS 中 interrupts 的信息(中断号) */
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return irq;
	}
	info->irq = irq;

	irq = platform_get_irq(pdev, 1);
	if (irq == -EPROBE_DEFER)
		return irq;

	info->tsirq = irq;

	info->dev = &pdev->dev;

	init_completion(&info->completion);

    //获取DTS中clock的信息
	info->clk = devm_clk_get(&pdev->dev, "adc");
	if (IS_ERR(info->clk)) {
		dev_err(&pdev->dev, "failed getting clock, err = %ld\n",
							PTR_ERR(info->clk));
		return PTR_ERR(info->clk);
	}

	if (info->data->needs_sclk) {
		info->sclk = devm_clk_get(&pdev->dev, "sclk");
		if (IS_ERR(info->sclk)) {
			dev_err(&pdev->dev,
				"failed getting sclk clock, err = %ld\n",
				PTR_ERR(info->sclk));
			return PTR_ERR(info->sclk);
		}
	}

	info->vdd = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(info->vdd)) {
		dev_err(&pdev->dev, "failed getting regulator, err = %ld\n",
							PTR_ERR(info->vdd));
		return PTR_ERR(info->vdd);
	}

	ret = regulator_enable(info->vdd);
	if (ret)
		return ret;

	ret = exynos_adc_prepare_clk(info);	
	if (ret) goto err_disable_reg;

	ret = exynos_adc_enable_clk(info);			/*使能时钟源*/
	if (ret) goto err_unprepare_clk;

	/********************************************************************/
	platform_set_drvdata(pdev, indio_dev);		/*设私有数据*/

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->dev.of_node = pdev->dev.of_node;
	indio_dev->info = &exynos_adc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;			/*提供 sysfs 接口*/
	indio_dev->channels = exynos_adc_iio_channels;	/*通道数据*/
	indio_dev->num_channels = info->data->num_channels;

	/* 在 Linux 内核中要想使用某个中断是需要申请的， request_irq 函数用于申请中断，
	注册中断处理函数，request_irq 函数可能会导致睡眠，
	因此在 中断上下文 或者 其他禁止睡眠 的代码段中 不能使用。
	request_irq 函数会激活(使能)中断，所以不需要我们手动去使能中断。 */
	/** 注册中断处理函数，使能中断(进程) */
	ret = request_irq(info->irq, exynos_adc_isr,
					0, dev_name(&pdev->dev), info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed requesting irq, irq = %d\n",
							info->irq);
		goto err_disable_clk;
	}

	ret = iio_device_register(indio_dev);/*注册iio设备*/
	if (ret) goto err_irq;

	if (info->data->init_hw) info->data->init_hw(info);/*ADC硬件初始化*/

	/* leave out any TS related code if unreachable */
	if (IS_REACHABLE(CONFIG_INPUT)) {
		has_ts = of_property_read_bool(pdev->dev.of_node,
					       "has-touchscreen") || pdata;
	}

	if (pdata) info->delay = pdata->delay;
	else info->delay = 10000;

	if (has_ts) ret = exynos_adc_ts_init(info);/*ADC硬件初始化*/
	if (ret) goto err_iio;

	ret = of_platform_populate(np, exynos_adc_match, NULL, &indio_dev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed adding child nodes\n");
		goto err_of_populate;
	}

	return 0;

err_of_populate:
	device_for_each_child(&indio_dev->dev, NULL,
				exynos_adc_remove_devices);
	if (has_ts) {
		input_unregister_device(info->input);
		free_irq(info->tsirq, info);
	}
err_iio:
	iio_device_unregister(indio_dev);
err_irq:
	free_irq(info->irq, info);
err_disable_clk:
	if (info->data->exit_hw) info->data->exit_hw(info);
	exynos_adc_disable_clk(info);
err_unprepare_clk:
	exynos_adc_unprepare_clk(info);
err_disable_reg:
	regulator_disable(info->vdd);
	return ret;
}

/*移除驱动*/
static int exynos_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct exynos_adc *info = iio_priv(indio_dev);

	if (IS_REACHABLE(CONFIG_INPUT) && info->input) {
		free_irq(info->tsirq, info);
		input_unregister_device(info->input);
	}
	device_for_each_child(&indio_dev->dev, NULL,
				exynos_adc_remove_devices);
	iio_device_unregister(indio_dev);/*注销iio设备*/
	free_irq(info->irq, info);
	if (info->data->exit_hw) info->data->exit_hw(info);
	exynos_adc_disable_clk(info);
	exynos_adc_unprepare_clk(info);
	regulator_disable(info->vdd);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_adc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct exynos_adc *info = iio_priv(indio_dev);

	if (info->data->exit_hw) info->data->exit_hw(info);
	exynos_adc_disable_clk(info);
	regulator_disable(info->vdd);

	return 0;
}

static int exynos_adc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct exynos_adc *info = iio_priv(indio_dev);
	int ret;

	ret = regulator_enable(info->vdd);
	if (ret)
		return ret;

	ret = exynos_adc_enable_clk(info);
	if (ret)
		return ret;

	if (info->data->init_hw) info->data->init_hw(info);/*ADC硬件初始化*/

	return 0;
}
#endif

/** *************** platform 驱动 结构体 ********************* **/

static SIMPLE_DEV_PM_OPS(exynos_adc_pm_ops,
			exynos_adc_suspend, /*悬挂（休眠）驱动*/
			exynos_adc_resume /*驱动恢复后要做什么*/
			);

static struct platform_driver exynos_adc_driver = {
	.probe = exynos_adc_probe,	 /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.remove = exynos_adc_remove, /*移除驱动*/
	.driver		= {
		.name	= "exynos-adc",		/* 名字，用于驱动和设备的匹配 */
		.of_match_table = exynos_adc_match,		/* 设备树匹配表 */
		.pm	= &exynos_adc_pm_ops,
	},
};

/********************************************************************/
#if 1
/*驱动模块自动加载、卸载函数*/
module_platform_driver(exynos_adc_driver);

#else
/*驱动模块加载函数*/
static int __init exynos_adc_init(void)
{
	printk(KERN_INFO "%s()\n", __func__);
	return platform_driver_register(&exynos_adc_driver); /*向系统注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit exynos_adc_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_INFO "%s()\n", __func__);
	platform_driver_unregister(&exynos_adc_driver); /*向系统注销驱动*/
}

module_init(exynos_adc_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(exynos_adc_exit); /*卸载驱动时运行的函数， 如 rmmod*/
#endif
/********************************************************************/

MODULE_LICENSE("GPL v2"); /* 声明开源，没有内核版本限制 */
MODULE_AUTHOR("Naveen Krishna Chatradhi <ch.naveen@samsung.com>");  /* 声明作者 */
MODULE_DESCRIPTION("Samsung EXYNOS ADC driver");

/** 工业场合里面也有大量的模拟量和数字量之间的转换，也就是我们常说的 ADC 和 DAC。
而且随着手机、物联网、工业物联网和可穿戴设备的爆发，传感器的需求只持续增强。比如手
机或者手环里面的加速度计、光传感器、陀螺仪、气压计、磁力计等，这些传感器本质上都是
ADC，大家注意查看这些传感器的手册，会发现他们内部都会有个 ADC，传感器对外提供 IIC
或者 SPI 接口， SOC 可以通过 IIC 或者 SPI 接口来获取到传感器内部的 ADC 数值，从而得到
想要测量的结果。Linux 内核为了管理这些日益增多的 ADC 类传感器，特地推出了 IIO 子系统  **/
