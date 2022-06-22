
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/i2c.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/regmap.h>

/**
针对 I2C 和 SPI 设备寄存器的操作都是通过相关
的 API 函数进行操作的。这样 Linux 内核中就会充斥着大量的重复、冗余代码，但是这些本质
上都是对寄存器的操作，所以为了方便内核开发人员统一访问 I2C/SPI 设备的时候，为此引入
了 Regmap 子系统

Linux 下大部分设备的驱动开发都是操作其内部寄存器，比如 I2C/SPI 设备的本质都是一样的，
通过 I2C/SPI 接口读写芯片内部寄存器。芯片内部寄存器也是同样的道理，
比如 I.MX6ULL的 PWM、定时器等外设初始化，最终都是要落到寄存器的设置上。
Linux 下使用 i2c_transfer 来读写 I2C 设备中的寄存器， 
SPI 接口的话使用 spi_write/spi_read等。 
I2C/SPI 芯片又非常的多，因此 Linux 内核里面就会充斥了大量的 i2c_transfer 这类的冗余代码，
再者，代码的复用性也会降低。比如 icm20608 这个芯片既支持 I2C 接口，
也支持 SPI 接口。假设我们在产品设计阶段一开始将 icm20608 设计为 SPI 接口，
但是后面发现 SPI 接口不够用，或者 SOC 的引脚不够用，我们需要将 icm20608 改为 I2C 接口。
这个时候 icm20608 的驱动就要大改，我们需要将 SPI 接口函数换为 I2C 的，工作量比较大。
基于代码复用的原则， Linux 内核引入了 regmap 模型， regmap 将寄存器访问的共同逻辑抽象出来，
驱动开发人员不需要再去纠结使用 SPI 或者 I2C 接口 API 函数，统一使用 regmap API函数。
这样的好处就是统一使用 regmap，降低了代码冗余， 提高了驱动的可以移植性。 

regmap模型的重点在于：
通过 regmap 模型提供的统一接口函数来访问器件的寄存器， SOC 内部的寄存器也可以使用 regmap 接口函数来访问。
regmap 是 Linux 内核为了减少慢速 I/O 在驱动上的冗余开销，提供了一种通用的接口来操作硬件寄存器。另外， regmap 在驱动和硬件之间添加了 cache，降低了低速 I/O 的操作次数，提高了访问效率，缺点是实时性会降低。

什么情况下会使用 regmap：
①、硬件寄存器操作，比如选用通过 I2C/SPI 接口来读写设备的内部寄存器，
或者需要读写 SOC 内部的硬件寄存器。
②、提高代码复用性和驱动一致性，简化驱动开发过程。
③、减少底层 I/O 操作次数，提高访问效率。

regmap 框架分为三层：
    regmap API 抽象层， regmap 向驱动编写人员提供的 API 接口，
驱动编写人员使用这些API 接口来操作具体的芯片设备，也是驱动编写人员重点要掌握的。
    regmap 核心层，用于实现 regmap，我们不用关心具体实现。
    底层物理总线： regmap 就是对不同的物理总线进行封装，
目前 regmap 支持的物理总线有 i2c、 i3c、
spi、 mmio、 sccb、 sdw、 slimbus、 irq、 spmi 和 w1。

Linux内核将regmap框架抽象为regmap结构体，这个结构体定义在 drivers/base/regmap/internal.h

regmap_config 结构体就是用来初始化 regmap 的，这个结构体也定义在 include/linux/regmap.h 


**/


