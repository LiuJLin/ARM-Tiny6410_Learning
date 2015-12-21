# Linux Module
OBJECT=char_leds
obj-m := $(OBJECT).o

#
CURRENT_PATH:=$(shell pwd)
#
LINUX_PATH:=/home/ljl/code/linux-kernel/linux-2.6.38

TARGET:
	make -C $(LINUX_PATH) M=$(CURRENT_PATH) modules
clean:
	make -C $(LINUX_PATH) M=$(CURRENT_PATH) clean
#send:
#	make
#	cp $(OBJECT).ko /var/nfsroot/rootfs/root
#	make clean
