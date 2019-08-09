#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/slab.h>
#define _BUF_MAX 1024*1024*4

static unsigned int major; 
static struct class *myclass = NULL;
static struct device *myclass_dev = NULL;
void *stuff;
static int my_drv_open(struct inode *inode, struct file *fl);
static ssize_t my_read(struct file *rfl, char __user *rbuf, size_t count, loff_t *lf);
static ssize_t my_write(struct file *wfl, const char __user *wbuf, size_t count, loff_t *lf);

static struct file_operations my_drv_fops = {
	.open = my_drv_open,
	.read = my_read,
	.write = my_write,
};

static  char val[_BUF_MAX] = {0};

static int my_drv_open(struct inode *inode, struct file *fl)
{
	printk("my_drv_open\r\n");
	return 0;
}

static ssize_t my_read(struct file *rfl, char __user *rbuf, size_t count, loff_t *lf)
{
	unsigned int buf_len = (unsigned int)(sizeof(val) / sizeof(char));
	unsigned int i;

	if(buf_len < count)
		count = buf_len;
	printk("my_drv read len :%ld\r\n",count);
	if(copy_to_user(rbuf, val, count))
	{
		return -EFAULT;
	}
	buf_len = count;
	for(i = 0; i < buf_len; i++)
		printk("my_drv read val[%d]= %d\r\n",i, val[i]);
	
	return 0;
}


static ssize_t my_write(struct file *wfl, const char __user *wbuf, size_t count, loff_t *lf)
{
	unsigned int buf_len = (unsigned int)(sizeof(val)/sizeof(char));
	unsigned int i;
	
	if(buf_len < count)
		count = buf_len;

	printk("my_drv write buf len:%ld\r\n",count);
	if(copy_from_user(val, wbuf, count))
	{
		return -EFAULT;
	}

	buf_len = count;
	for(i = 0; i< buf_len; i++)
		printk("my_drv write val[%d]=%d\r\n",i,val[i]);
	
	return 0;
}

unsigned char *vmallocbuf;

int my_drv_init(void)
{
	unsigned int addr;
	major =	register_chrdev(0, "my_drv", &my_drv_fops);

	myclass = class_create(THIS_MODULE, "my_drv");
	if (IS_ERR(myclass))
		goto class_fail;
	
	myclass_dev = device_create(myclass,NULL, MKDEV(major, 0),NULL,"my_drv");
	if(IS_ERR(myclass_dev)) 
		goto dev_fail;

	stuff = kmalloc(9,GFP_KERNEL);	
	printk("kmalloc got : %zu byte of memory\n",ksize(stuff));
	printk("<1>kmalloc: kmallocmem va addr=%p  "
			            "\tpa addr=%lx\n", stuff, __pa(stuff));

	addr = __pa(stuff);
	printk("va add =%p\n",__va(addr));
	kfree(stuff);

	vmallocbuf =(unsigned char *) vmalloc(8*1024*PAGE_SIZE);
	printk("page size =%ld\n",PAGE_SIZE);
	printk("<2>vmalloc: vmallocmem va addr=%p\n", vmallocbuf);
	vfree(vmallocbuf);
	return 0;


dev_fail:
	class_destroy(myclass);
class_fail:
	unregister_chrdev(major, "my_drv");

	return -1;
}

void my_drv_exit(void)
{
	device_destroy(myclass,MKDEV(major, 0));
	class_destroy(myclass);
	unregister_chrdev(major, "my_drv");
}

module_init(my_drv_init);
module_exit(my_drv_exit);
MODULE_AUTHOR("XZB");
MODULE_VERSION("0.0.0");
MODULE_LICENSE("GPL");




