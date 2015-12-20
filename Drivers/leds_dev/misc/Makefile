# Linux Module
obj-m := leds_dev.o
#当前路径
CURRENT_PATH:=$(shell pwd)
# 交叉编译的linux kernel
LINUX_PATH:=/home/ljl/code/linux-kernel/linux-2.6.38

TARGER:
	make -C $(LINUX_PATH) M=$(CURRENT_PATH) modules
clean:
	make -C $(LINUX_PATH) M=$(CURRENT_PATH) modules
