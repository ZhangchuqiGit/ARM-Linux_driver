/* industrial I/O data types needed both in and out of kernel
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef _IIO_TYPES_H_
#define _IIO_TYPES_H_

#include <uapi/linux/iio/types.h>

enum iio_event_info {
	IIO_EV_INFO_ENABLE,
	IIO_EV_INFO_VALUE,
	IIO_EV_INFO_HYSTERESIS,
	IIO_EV_INFO_PERIOD,
	IIO_EV_INFO_HIGH_PASS_FILTER_3DB,
	IIO_EV_INFO_LOW_PASS_FILTER_3DB,
};

#define IIO_VAL_INT 1
#define IIO_VAL_INT_PLUS_MICRO 2
#define IIO_VAL_INT_PLUS_NANO 3
#define IIO_VAL_INT_PLUS_MICRO_DB 4
#define IIO_VAL_INT_MULTIPLE 5
#define IIO_VAL_FRACTIONAL 10
#define IIO_VAL_FRACTIONAL_LOG2 11

/**		数据组合表 
IIO_VAL_INT				整数值，没有小数。比如 5000，那么就是 val=5000，不需要设置 val2 
IIO_VAL_INT_PLUS_MICRO	小数部分扩大 1000000 倍，比如 1.00236，此时 val=1，val2=2360。
IIO_VAL_INT_PLUS_NANO	小数部分扩大 1000000000 倍，同样是 1.00236，
					此时 val=1， val2=2360000。
IIO_VAL_INT_PLUS_MICRO_DB	dB 数据，和 IIO_VAL_INT_PLUS_MICRO 数据形式一样，
					只是在后面添加 db。
IIO_VAL_INT_MULTIPLE	多个整数值，比如一次要传回 6 个整数值，那么 val 和 val2 就不够用了。
					此宏主要用于 iio_info 的 read_raw_multi 函数。
IIO_VAL_FRACTIONAL	分数值，也就是 val/val2。比如 val=1， val2=4，那么实际值就是 1/4。
IIO_VAL_FRACTIONAL_LOG2	值为 val>>val2，也就是 val 右移 val2 位。
					比如 val=25600，val2=4， 那么真正的值就是 25600 右移 4 位，
					25600>>4=1600. */

enum iio_available_type {
	IIO_AVAIL_LIST,
	IIO_AVAIL_RANGE,
};

#endif /* _IIO_TYPES_H_ */
