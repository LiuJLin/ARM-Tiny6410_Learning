/*************************************************************

     Description:	Friendly ARM Tiny6410 - Linux Device Drivers - LED - 字符混杂设备miscdevice驱动编写


*************************************************************/
// {{{ include file
#include <linux/module.h>  //THIS_MODULE, MODULE_AUTHOR, MODULE_LICENSE
#include <linux/init.h>    //module_init, module_exit
#include <linux/fs.h>      //struct file_operation; 系统调用与驱动程序的映射；
			   //open(),read(),write(),ioctl()
			   //
#include <linux/miscdevice.h>//miscdevice 驱动程序
#include <asm/io.h>        //ioread32, iowrite32
#include <mach/gpio-bank-k.h>//S3C6410XX_GPKCON, S3C64XX_GPKDAT
#include <mach/regs-gpio.h>//定义了gpio-bank-k中是用的S3C64XX_GPK_BASE
#include <mach/map.h> //定义了S3C64XX_VA_GPIO
// }}}

// {{{ Device Name
// 加载模式后，执行"cat /proc/devices"将会看到的设备名称
#define DEVICE_NAME		"ledsdev"
// }}}

// {{{ unlocked_ioctl
// long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);// unlocked_ioctl: 设备驱动程序中对设备的I/O通道进行管理的函数
// 用户程序通过命令cmd 告知驱动程序想要进行的操作，而驱动程序则负责解释这些命令并实现这些命令
// 参数:  cmd = 0, turn off the selected led;
//        cmd = 1, turn on  the selected led;
//        arg: 0~4 (0:all ,1~4: led1~led4)
// 返回： 0或者  错误 -EINVAL
static long s3c6410_leds_ioctl(struct file* flip, unsigned int cmd, unsigned long arg)
{
	switch(cmd)
	{
		unsigned tmp;
		case 0:
		case 1://open
		{
			//判断参数arg
			if(arg > 4)  //参数错误
			{
				return -EINVAL;
			}
			else if(arg == 0)  // all leds
			{
				//LED1~4 -> GPKDAT[4:7]
				tmp = ioread32(S3C64XX_GPKDAT);//读取GPKDAT寄存器中LED1~4 所对应位的值
				if(cmd == 1)  // turn on, set 0
					tmp &= ~(0xF<<4);
				else if(cmd == 0)  // turn off, set 1
					tmp |= (0xF<<4);
				iowrite32(tmp, S3C64XX_GPKDAT);//将改变的值写入对应寄存器
			}
			else 
			{
				tmp = ioread32(S3C64XX_GPKDAT);//读取GPKDAT寄存器中LED1~4 所对应位的值
				tmp &= ~(1 << (arg+3));//清除该位，使该位置为0，亮
				tmp |= ((!cmd) << (arg+3));//当cmd=1：该位为零，亮；当cmd=0;该位为1，灭
			}
			return 0;
		}
		default:
			return -EINVAL;
	}
	return 0;//该语句应该不会执行，在此为了对应static int返回值，防止编译器警告；
}
// }}}

// {{{ file_operations
static struct file_operations S3C6410_LEDS_FOPS =
{
	// #define THIS_MODULE (&__this_module);__this_module在编译模块时自动创建
	.owner = THIS_MODULE, //一个宏
	.unlocked_ioctl = s3c6410_leds_ioctl,
	//在此，led实现一个ioctl函数即可
};
// }}}

// {{{ miscdevice
static struct miscdevice misc =
{
	.minor = MISC_DYNAMIC_MINOR,  //动态分配次设备号
	.name = DEVICE_NAME,	     //设备名称
	.fops = &S3C6410_LEDS_FOPS,
};
// }}}

// {{{ module init
// 执行insmod命令时就会调用此函数
// static int __init init_module(void) //默认驱动初始化函数，不用默认函数就要添加宏module_init()
// __init 宏，只有静态链接驱动到内核时才有用，表示将此函数代码放在".init.text"段中；使用一次后释放这段内存；
static int __init s3c6410_leds_init(void)
{
	//设备驱动程序注册时，初始化LED1-4对应的GPIO管脚为输出状态，且熄灭LED1-4
	int ret;  //存放函数调用返回值
	unsigned long tmp;

	//配置 相应IO为输出
	tmp = ioread32(S3C64XX_GPKCON);
	tmp &= ~(0xFFFF0000); //  设置GPKCON[16:31]
	tmp |= (0x11110000);  //  设置为输出
	iowrite32(tmp,S3C64XX_GPKCON);//写入GPKCON

	//turn off all leds
	tmp = ioread32(S3C64XX_GPKDAT);//readl,readw,readb,writeb,writew,writel  旧的读写，不推荐
	tmp |= (0xF << 4);
	iowrite32(tmp,S3C64XX_GPKDAT);

	//注册misc
	//misc_device 是特殊的字符设备，注册驱动程序时采用misc_register函数注册
	//此函数可以自动创建设备节点，无需mknod指令创建设备文件
	ret = misc_register(&misc);
	if (ret < 0)
	{
		printk(DEVICE_NAME "can't register misc device - LED Driver\n");
		return ret;
	}
	printk(DEVICE_NAME "initialized.\n");


	return 0;
}
// }}}

// {{{ module exit
// 执行rmmod时就会调用本函数
// static int __exit cleanup_module(void)  //默认清除函数
// __exit 宏，表示将此函数代码放在".exit.data"段中；静态链接时没有使用，因为静态链接的驱动不能被卸载  ？？？？
static void __exit s3c6410_leds_exit(void)
{
	//卸载驱动程序
	misc_deregister(&misc);
}
// }}}

//指定驱动程序的初始化函数和卸载函数
module_init(s3c6410_leds_init);
module_exit(s3c6410_leds_exit);

// {{{ Module Description
// 非必须
MODULE_VERSION("000");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Tiny6410 LED Driver-MISC");
// }}}



