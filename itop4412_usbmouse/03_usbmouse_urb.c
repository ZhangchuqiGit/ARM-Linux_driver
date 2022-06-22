
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

/********************************************************************/

#define DRIVER_VERSION "v1.6"

int pipe, maxp, count;
signed char *data;
struct urb *mouse_urb;
dma_addr_t data_dma;

/********************************************************************/

/* usb_complete_t 指向完成处理者函数, 当 urb 被完全传输或发生错误时， 被调用 */
static void check_usb_data(struct urb *urb)
{
	printk("count is %d time!\n", count++);

	usb_submit_urb(mouse_urb, GFP_KERNEL);
}

/********************************************************************/

/*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
static int usb_mouse_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_endpoint_descriptor *endpoint;

	printk(KERN_INFO "%s()\n", __func__);

	endpoint = &intf->cur_altsetting->endpoint[0].desc;
	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

/*申请内存空间用于数据传输， data 为指向该空间的地址， data_dma 则是这块内存空间的 dma 映射， 即
这块内存空间对应的 dma 地址。 在使用 dma 传输的情况下， 则使用 data_dma 指向的 dma 区域， 否则
使用 data 指向的普通内存区域进行传输。 GFP_ATOMIC 表示不等待， GFP_KERNEL 是普通的优先级， 可以
睡眠等待， 由于鼠标使用中断传输方式， 不允许睡眠状态， data 又是周期性获取鼠标事件的存储区， 因此
使用 GFP_ATOMIC 优先级， 如果不能 分配到内存则立即返回 0*/
	data = usb_alloc_coherent(dev, 8, GFP_ATOMIC, &data_dma);

	mouse_urb = usb_alloc_urb(0, GFP_KERNEL);

	usb_fill_int_urb(mouse_urb, dev, pipe, data,
					 (maxp > 8 ? 8 : maxp), check_usb_data,
					 NULL, endpoint->bInterval);

	mouse_urb->transfer_dma = data_dma;
	mouse_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	usb_submit_urb(mouse_urb, GFP_KERNEL);

	return 0;
}

static void usb_mouse_disconnect(struct usb_interface *intf)
{
	printk(KERN_INFO "%s()\n", __func__);
}

/** *************** USB 设备 匹配列表 ********************* **/

static struct usb_device_id usb_mouse_id_table[] = {
	{USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
						USB_INTERFACE_PROTOCOL_MOUSE)},
	{} /* Terminating entry */
};
/*MODULE_DEVICE_TABLE
一是将设备加入到外设队列中，
二是告诉程序阅读者该设备是热插拔设备或是说该设备支持热插拔功能*/
MODULE_DEVICE_TABLE(usb, usb_mouse_id_table);

/** *************** USB 驱动 结构体 ********************* **/

static struct usb_driver usb_mouse_driver = {
	.name = "usbmouse_03",
	.probe = usb_mouse_probe, /*匹配设备时加载驱动,当驱动与设备匹配以后此函数就会执行*/
	.disconnect = usb_mouse_disconnect,
	.id_table = usb_mouse_id_table,
};

/********************************************************************/
#if 0
module_usb_driver(usb_mouse_driver);
#else
/*驱动模块加载函数*/
static int __init usb_mouse_init(void)
{
	printk(KERN_EMERG "%s()\n", __func__);
	return usb_register(&usb_mouse_driver); /*注册驱动*/
}

/*驱动模块卸载函数*/
static void __exit usb_mouse_exit(void)
{
	/*打印信息, KERN_EMERG 表示紧急信息*/
	printk(KERN_EMERG "%s()\n", __func__);
	usb_deregister(&usb_mouse_driver); /*注销驱动*/
}
#endif
/********************************************************************/

module_init(usb_mouse_init); /*加载驱动时运行的函数,  如 insmod*/
module_exit(usb_mouse_exit); /*卸载驱动时运行的函数， 如 rmmod*/

MODULE_LICENSE("GPL"); /*声明开源的，没有内核版本限制*/
MODULE_AUTHOR("zcq");  /*声明作者*/
MODULE_DESCRIPTION("zcq USB HID Boot Protocol mouse driver");
