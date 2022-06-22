#ifndef AP3216C_H
#define AP3216C_H
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: ap3216creg.h
作者	  	: 左忠凯
版本	   	: V1.0
描述	   	: AP3216C寄存器地址描述头文件
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2019/9/2 左忠凯创建
***************************************************************/

/** 集成 数字环境光传感器（ALS），距离传感器（PS），红外LED（IR）
 * 适合做自动调光，手势识别等   */

#define AP3216C_ADDR    	0X1E	/* AP3216C器件地址  */

/* AP3316C寄存器 */

#define AP3216C_SYSTEMCONG	0x00	/* 配置寄存器  系统模式/见 百度搜索    */
#define PowerDown 				0x0	// 掉电模式（默认）
#define MODE_ALS 				0x1	// ALS 功能
#define MODE_PS_IR				0x2	// PS+IR 功能
#define MODE_ALS_PS_IR 			0x3	// ALS+PS+IR 功能
#define SoftReset 				0x4	// 软复位
#define Single_MODE_ALS ALS 	0x5 // 单次模式
#define Single_MODE_PS_IR  		0x6 // 单次模式
#define Single_MODE_ALS_PS_IR 	0x7 // 单次模式

#define AP3216C_INTSTATUS	0X01	/* 中断状态寄存器   */
#define AP3216C_INTCLEAR	0X02	/* 中断清除寄存器   */

#define AP3216C_IRDATALOW	0x0A	/* IR数据低字节     */
#define AP3216C_IRDATAHIGH	0x0B	/* IR数据高字节     */

#define AP3216C_ALSDATALOW	0x0C	/* ALS数据低字节    */
#define AP3216C_ALSDATAHIGH	0X0D	/* ALS数据高字节    */

#define AP3216C_PSDATALOW	0X0E	/* PS数据低字节     */
#define AP3216C_PSDATAHIGH	0X0F	/* PS数据高字节     */

#endif

