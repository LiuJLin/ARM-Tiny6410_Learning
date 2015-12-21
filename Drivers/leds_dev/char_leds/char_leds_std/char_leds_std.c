/****************************************************************
	Description: Friendly ARM Tiny6410 - Linux Device Drivers - LED- 标准字符设备驱动程序
	> 需要手动创建设备节点
		#insmod char_leds.ko 得到分配的设备号，假设为253，则创建设备节点
		#mknod /dev/ledsdev c 253 0
		
	> LED I/O 在open()时设置为输出状态
	> 应用程序通过ioctl发送cmd(0:off,1:on)和arg(led No.)实现各led亮灭

*****************************************************************/

// {{{
#include <linux/module.h> //THIS_MODULE, MODULE_AUTHOR,MODULE_LISENCE..
#include <linux/init.h>   //module_init,module_exit
#include <linux/fs.h>     //struct file_operations;
			  //open(),read(),write(),ioctl()..
			  //

#include <asm/io.h>	 //ioread32,iowrite32
#include <mach/gpio-bank-k.h> //S3C64XX_GPKCON, S3C64XX_GPKDAT
#include <mach/regs-gpio.h>  //gpio-bank-k中使用的S3C64XX_GPK_BASE
#include <mach/map.h>  //S3C64XX_VA_GPIO
#include <linux/cdev.h> //包含字符驱动设备struct cdev的定义
#include <asm/uaccess.h> //copy_to_user() & copy_from_user()
// }}}

// {{{ Device Name
// 加载模式后，执行"cat /proc/devices"命令看到的设备名称
#define DEVICE_NAME  "ledsdev_char_std"
// 设备编号，为0时会自动分配
#define DEVICE_NAME   0
// }}}

// {{{ cdev
// 每个字符设备都对应一个cdev结构体变量.
// cdev一般它有两种定义初始化方式：
// > 静态  内存定义初始化：
//		struct cdev my_cdev;//创建cdev结构体变量
//		cdev_init(&my_cdev, &fops);//绑定file_operations
//		my_cdev.owner = THIS_MODULE;//配置owner
// > 动态  内存定义初始化：
//		struct cdev *my_cdev = cdev_alloc();
//		my_cdev->ops = &fops;
//		my_cdev->owner = THIS_MODULE;
// 两种方式的功能时一样的，只是是用的内存区不同，一般视实际的数据结构需求而定

/*****************<linux-2.6.38/include/linux/cdev.h>*****************
	struct cdev
	{
		struct kobject kobj;//内嵌的内核对象
		struct module *owner;//指向内核模块对象的指针
		const struct file_operations *ops;//
		struct list_head list;
		dev_t dev; //字符设备的设备号
		unsigned int count;//次设备号的个数
	}
********************************************************************/

static struct cdev leds_cdev;//定义一个字符设备对象
static dev_t leds_dev_t;//字符设备节点的设备号
// }}}

// {{{ open
// int (*open)(struct inode *inode, struct file *filp)
// open方法提供给驱动程序以初始化的能力,为完成初始化做准备
static int leds_status = 0;
static int s3c6410_leds_open(struct inode *nd, struct file *filp)
{
	unsigned long tmp;
	if(leds_status == 0)
	{
		//配置LED IO为输出
		tmp = ioread32(S3C64XX_GPKCON);
		tmp &= ~(0xFFFF0000); //清零GPKCON0[16:31]
		tmp |= (0x11110000);  //设置为输出
		iowrite32(tmp,S3C64XX_GPKCON); //写入GPKCON

		//熄灯LED灯
		tmp = readl(S3C64XX_GPKDAT);  //旧的io读写，不推荐使用；readb,readw,readl,writeb,writew,writel.
		tmp |= (0xF<<4);              //关闭LED灯
		iowrite32(tmp,S3C64XX_GPKDAT);
		leds_status = 1;
	}
	printk(DEVICE_NAME ", open char device.\n");
	return 0;
}
// }}}

// {{{ close
static int s3c6410_leds_close(struct inode *nd, struct file *filp)
{
	printk(DEVICE_NAME ", close char device.\n");
	return 0;
}
// }}}

// {{{ unlocked_ioctl
// long (*unclocked_ioctl) (struct file *, unsigned int, unsigned long);
// ioctl 是设备驱动程序中对设备的I/O 通道进行管理的函数
static long s3c6410_leds_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch(cmd)
	{
		unsigned tmp;
		case 0://close
		case 1://open
		{
			//判断参数arg
			if(arg > 4)
			{
				return -EINVAL;
			}
			else if(arg == 0)  // 处理所有的LED灯
			{
				// LED1~4 -> GPKDAT[4:7]
				tmp = ioread32(S3C64XX_GPKDAT);   
				if(cmd == 1)
					tmp &= ~(0xF<<4);
				else if(cmd == 0)
					tmp |= (0xF<<4);
				iowrite32(tmp, S3C64XX_GPKDAT);
			}
			else
			{
				tmp = ioread32(S3C64XX_GPKDAT);
				tmp &= ~(1 << (3+arg));
				tmp |= ((!cmd) << (3+arg));
				iowrite32(tmp, S3C64XX_GPKDAT);
			}
			return 0;
		}
		default:
			return -EINVAL;
	}
	return 0;
}
// }}}


// {{{ read
// 返回led灯的状态
static ssize_t s3c6410_leds_read(struct file *filep, char __user *buf, size_t count, loff_t *off)
{
	volatile unsigned long tmp = 0;
	size_t failed_copy_count = 0;

	// 读取GPKDAT
	tmp = ioread32(S3C64XX_GPKDAT);
	tmp = (tmp & (0xF<<4)) >> 4; // GPKDAT[4:7] -> tmp[0:3]
	// 拷贝到用户空间
	failed_copy_count = copy_to_user((char*)buf,(char*)&tmp, 4);
	if(failed_copy_count != 0)
		return -EFAULT;
	else
		return 0;
}
// }}}

// {{{ file_operations
static struct file_operations S3C6410_LEDS_FOPS =
{
	.owner	=  THIS_MODULE,
	.open	=  s3c6410_leds_open,
	.release=  s3c6410_leds_close,
	.read	=  s3c6410_leds_read,
	.unlocked_ioctl = s3c6410_leds_ioctl,
};
// }}}

// {{{ module init
// 执行insmod命令时便会调用此函数
// static int __init init_module(void) //默认驱动初始化函数，若不用默认函数就要添加宏module_init()
// __init 宏，只有静态链接驱动到内核时才有用，表示将此函数代码放在".init.text"段中；使用一次之后释放这段内存；
static int __init s3c6410_leds_init(void)
{
	int ret;
	printk(DEVICE_NAME ", char device init.\n");
	
	if (DEVICI_MAJOR)
	{
		leds_dev_t = MKDEV(DEVICE_MAJOR, 0);//通过主次设备号来生成dev_t类型的设备号
		ret = register_chrdev_region(leds_dev_t, 1, DEVICE_NAME);//
		if(ret < 0)  // register failed
		{
			printk(KERN_WARNING "register device numbers error.\n");
			return ret;
		}
	}
	else  //DEVICE_NAME == 0, 自动分配设备号并注册
	{
		//int alloc_chrdev_region(dev_t *dev, unsigned int firstminor, unsigned int count, char *name);
		int leds_major = 0;
		ret = alloc_chrdev_region(&leds_dev_t, 0, 1, DEVICE_NAME);
		if(ret < 0)
		{
			printk(KERN_WARNING "alloc char device number error\n");
			return ret;
		}
		leds_major = MAJOR(leds_dev_t);
		printk(KERN_ALERT "leds device major = %d.\n",leds_major);
	}

	cdev_init(&leds_cdev, &S3C6410_LEDS_FOPS);
	cdev_add(&leds_dev, leds_dev_t, 1);
	
	return 0;
}
// }}}

// {{{ module exit
// 执行rmmod命令时调用此函数
// static int __exit cleanup_module(void)  //默认驱动清除函数
// __exit 宏，表示将此函数代码放在".exit.data"段
static void __exit s3c6410_leds_exit(void)
{
	//卸载驱动程序
	printk(DEVICE_NAME " module exit.\n");
	cdev_del(&leds_cdev);// 注销chr_dev对应的字符设备对象

	unregister_chrdev_region(leds_dev_t, 1);//释放分配的设备号
}
// }}}

//指定驱动程序的初始化和卸载函数
module_init(s3c6410_leds_init);
module_exit(s3c6410_leds_exit);

// {{{ module description)
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Tiny6410 LED_char Driver");
// }}}

























































