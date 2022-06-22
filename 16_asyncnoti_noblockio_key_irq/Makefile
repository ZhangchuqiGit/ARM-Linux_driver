
# kernel 源码 目录
KERNELDIR ?= /home/zcq/ARM_Linux_iTOP-4412/zcq_itop4412_POP1G_kernel-4.14.2

# 编译模块
CODE_Dirver ?=
#obj-m += mini_linux_module.o
obj-m += $(CODE_Dirver)

###############################################################################

#当前目录变量
CURRENT_PATH ?= $(shell pwd)

#make命名默认寻找第一个目标
#make -C就是指调用执行的路径
#$(KERNELDIR)Linux源码目录，作者这里指的是/home/topeet/android4.0/iTop4412_Kernel_3.0
#$(CURRENT_PATH)当前目录变量
#modules要执行的操作

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
		
#make clean执行的操作是删除后缀为o的文件
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
	rm -rf *.mod
