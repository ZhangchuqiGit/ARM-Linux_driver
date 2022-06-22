
/** 01 鼠标驱动注册
在 Linux 的 USB 驱动中， 鼠标驱动是最简单， 最适合用来学习的。
USB 鼠标驱动是 drivers/hid/usbhid/usbmouse.c 文件，
这个驱动在任意版本的内核中都有。
我们将“usbmouse.c” 文件进行分解， 提取其中一部分来分析， 
本文档主要分析 USB 鼠标驱动的注册部分。   */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

#define DRIVER_VERSION "v1.6"

/********************************************************************/

/*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
static int usb_mouse_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	printk(KERN_INFO "%s()\n", __func__);
	return 0;
}
static void usb_mouse_disconnect(struct usb_interface *intf)
{
	printk(KERN_INFO "%s()\n", __func__);
}

/** *************** USB 设备 匹配列表 ********************* **/

static struct usb_device_id usb_mouse_id_table [] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE) },
	{ }	/* Terminating entry */
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE (usb, usb_mouse_id_table);

/** *************** USB 驱动 结构体 ********************* **/

static struct usb_driver usb_mouse_driver = {
	.name		= "usbmouse",
	.probe		= usb_mouse_probe, /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.disconnect	= usb_mouse_disconnect,
	.id_table	= usb_mouse_id_table,
};

/********************************************************************/
#if 0
module_usb_driver(usb_mouse_driver);
#else
/*驱动模块加载函数*/
static int __init usb_mouse_init(void)
{
	printk(KERN_EMERG "%s()\n", __func__);
	return usb_register(&usb_mouse_driver);	/*注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit usb_mouse_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s()\n", __func__);
	usb_deregister(&usb_mouse_driver);		/*注销驱动*/
}
#endif
/********************************************************************/

module_init(usb_mouse_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(usb_mouse_exit); /*卸载驱动时运行的函数， 如 rmmod*/

MODULE_LICENSE("GPL"); 	/*声明开源的，没有内核版本限制*/
MODULE_AUTHOR("zcq");	/*声明作者*/
MODULE_DESCRIPTION("zcq USB HID Boot Protocol mouse driver");
