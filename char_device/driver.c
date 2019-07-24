#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

#define _BUF_MAX 1024*1024*4
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

int my_drv_init(void)
{
	int major_id = 100;
	register_chrdev(major_id, "my_drv", &my_drv_fops);
	return 0;
}

void my_drv_exit(void)
{
	unregister_chrdev(100, "my_drv");
}

module_init(my_drv_init);
module_exit(my_drv_exit);
MODULE_AUTHOR("XZB");
MODULE_VERSION("0.0.0");
MODULE_LICENSE("GPL");




