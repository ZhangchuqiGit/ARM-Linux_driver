/**
adc_demo @126C0000
{
    compatible = "tiny4412,adc_demo";
    reg = <0x126C 0x20>;
    clocks = <&clock CLK_TSADC>;
    clock - names = "timers";
    interrupt - parent = <&combiner>;
    interrupts = <10 3>;
}*/

/********************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/interrupt.h>

/********************************************************************/

DECLARE_WAIT_QUEUE_HEAD(wait);

static int major;
static struct cdev adc_cdev;
static struct class *cls;

/** itop4412 一共有 4 路 ADC 接口
	* 网络标号是 XadcAIN0~XadcAIN3 
	* 开发板自带的 ADC 电路， ADC 接的是滑动变阻器
	Power Supply Voltage: 1.8V (Typ.), 1.0V (Typ., Digital I/O Interface)
	Analog Input Range: 0 ~ 1.8V 
	Exynos 4412 has two ADC blocks, ADC_CFG[16] setting :
		General ADC : 0x126C_0000
		MTCADC_ISP :  0x1215_0000  
	sec_exynos4412_users manual_ver.1.00.00.pdf; [57] ADC [57.7] p2770 */

/* S3C/EXYNOS4412/5250 ADC_V1 registers definitions */
#define ADC_V1_CON(x) ((x) + 0x00)
#define ADC_V1_TSC(x) ((x) + 0x04)
#define ADC_V1_DLY(x) ((x) + 0x08)
#define ADC_V1_DATX(x) ((x) + 0x0C)
#define ADC_V1_DATY(x) ((x) + 0x10)
#define ADC_V1_UPDN(x) ((x) + 0x14)
#define ADC_V1_INTCLR(x) ((x) + 0x18)
#define ADC_V1_MUX(x) ((x) + 0x1c) /*	ADCMUX[3:0] 指定模拟输入通道选择 \
0000 = AIN 0;	0001 = AIN 1;	0010 = AIN 2;	0011 = AIN 3  */

struct ADC_BASE
{
    unsigned int ADCCON;    //0
    unsigned int temp0;     //
    unsigned int ADCDLY;    //8
    unsigned int ADCDAT;    //c
    unsigned int temp1;     //10
    unsigned int temp2;     //14
    unsigned int CLRINTADC; //18
    unsigned int ADCMUX;    //1c
};
volatile static struct ADC_BASE *adc_base = NULL;

/********************************************************************/

static int adc_open(struct inode *inode, struct file *file)
{
    printk("adc_open\n");
    return 0;
}

static int adc_release(struct inode *inode, struct file *file)
{
    printk("adc_exit\n");
    return 0;
}

static ssize_t adc_read(struct file *filp, char __user *buf, size_t count, loff_t *offt)
{
    int data = 0, ret = 0;

    printk("adc_read\n");
    adc_base->ADCMUX = 0x00;
    adc_base->ADCCON = (1 << 16 | 1 << 14 | 99 << 6 | 1 << 0);
    wait_event_interruptible(wait, ((adc_base->ADCCON >> 15) & 0x01));
    data = adc_base->ADCDAT & 0xfff;

    ret = raw_copy_to_user(buf, &data, count);
    printk("copy_to_user %x\n", data);

    if (ret < 0)
    {
        printk("copy_to_user error\n");
        return -EFAULT;
    }

    return count;
}

/********************************************************************/

static struct file_operations adc_fops =
    {
        .owner = THIS_MODULE,
        .open = adc_open,
        .read = adc_read,
        .release = adc_release,
};

/********************************************************************/

static irqreturn_t adc_demo_isr(int irq, void *dev_id)
{
    printk("enter irq now to wake up\n");
    wake_up(&wait);
    /* clear irq */
    adc_base->CLRINTADC = 1;
    return IRQ_HANDLED;
}

struct clk *base_clk;
int irq;

/** *************** platform 驱动 结构体 函数 ********************* **/

static int adc_probe(struct platform_device *pdev)
{
    dev_t devid;
    struct device *dev = &pdev->dev;
    struct resource *res = NULL;
    int ret;

    res = platform_get_resource(pdev, IORESOURCE_MEM /*获取DTS中reg的信息*/, 0);
    if (res == NULL)    {
        printk("platform_get_resource error\n");
        return -EINVAL;
    }
    printk("res: %x\n", (unsigned int)res->start);

    //将获取到的reg的信息，进行ioremap，为后续操作做准备
    adc_base = devm_ioremap_resource(&pdev->dev, res);
    if (adc_base == NULL)    {
        printk("devm_ioremap_resource error\n");
        goto err_clk;
    }
    printk("adc_base: %x\n", (unsigned int)adc_base);

    //获取DTS中clock的信息
    base_clk = devm_clk_get(&pdev->dev, "adc");
    if (IS_ERR(base_clk))    {
        dev_err(dev, "failed to get timer base clk\n");
        return PTR_ERR(base_clk);
    }

    ret = clk_prepare_enable(base_clk);
    if (ret < 0)    {
        dev_err(dev, "failed to enable base clock\n");
        return -EINVAL;
    }

    irq = platform_get_irq(pdev, 0);
    if (irq < 0)    {
        dev_err(&pdev->dev, "no irq resource?\n");
        goto err_clk;
    }

    ret = request_irq(irq, adc_demo_isr, 0, "adc", NULL);
    if (ret < 0)    {
        dev_err(dev, "failed to request_irq\n");
        goto err_clk;
    }

    /** 没有定义设备号, 动态申请设备号 */
    if (alloc_chrdev_region(&devid, 0, 1, "adc") < 0)    {
        printk("%s ERROR\n", __func__);
        goto err_req_irq;
    }

    major = MAJOR(devid);
    cdev_init(&adc_cdev, &adc_fops);
    cdev_add(&adc_cdev, devid, 1);
    cls = class_create(THIS_MODULE, "myadc");
    device_create(cls, NULL, MKDEV(major, 0), NULL, "adc");
    return 0;

err_req_irq:
    free_irq(irq, NULL);
err_clk:
    clk_disable(base_clk);
    clk_unprepare(base_clk);
    return -EINVAL;
}

static int adc_remove(struct platform_device *pdev)
{
    printk("enter %s\n", __func__);
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    cdev_del(&adc_cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);
    clk_disable(base_clk);
    clk_unprepare(base_clk);
    free_irq(irq, NULL);
    printk("%s enter.\n", __func__);
    return 0;
}

/********************************************************************/

static const struct of_device_id adc_dt_ids[] = {
    {.compatible = "itop4412,adc_demo"}, /* 兼容属性 */
    {},
};

MODULE_DEVICE_TABLE(of, adc_dt_ids);

static struct platform_driver adc_driver =
    {
        .driver = {
            .name = "adc_demo",
            .of_match_table = of_match_ptr(adc_dt_ids),
        },
        .probe = adc_probe,
        .remove = adc_remove,
};

/********************************************************************/

static int adc_init(void)
{
    int ret;
    printk("enter %s\n", __func__);
    ret = platform_driver_register(&adc_driver);

    if (ret)
    {
        printk(KERN_ERR "adc demo: probe faiadc: %d\n", ret);
    }

    return ret;
}

static void adc_exit(void)
{
    printk("enter %s\n", __func__);
    platform_driver_unregister(&adc_driver);
}

module_init(adc_init);
module_exit(adc_exit);
