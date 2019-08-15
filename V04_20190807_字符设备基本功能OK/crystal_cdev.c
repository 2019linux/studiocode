#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/sched/signal.h>



#define DEVICE_NAME			"crystal_cdev"
#define BUFFERSIZE			4*1024*1024		//全局内存最大4M字节
#define MEM_CLEAR			0x1				//清0全部存储
#define CRYSTAL_CDEV_MAJOR	137				//预设的crystal_cdev的主设备号

static int buffersize = BUFFERSIZE;
static int crystal_cdev_major = CRYSTAL_CDEV_MAJOR;
static struct class *crystalclass;
static unsigned int variable;
static struct proc_dir_entry *crystal_cdev_dir, *crystal_cdev_entry;
static bool crystal_cdev_debug = 1;			//默认debug模式，输出调试log

#define CRYSTAL_DEBUG(fmt, arg...)	do {\
	if (crystal_cdev_debug)\
		printk("<<-CRYSTAL_CDEV-DEBUG->> [%d]"fmt"\n", __LINE__, ##arg);\
} while (0)

#define CRYSTAL_ERROR(fmt, arg...)	do {\
	printk("<<-CRYSTAL_CDEV-ERROR->> [%d]"fmt"\n", __LINE__, ##arg);\
} while (0)

/*crystal_cdev设备结构体*/
struct crystal_cdev_dev
{
	struct cdev cdev;					//cdev结构体   
	unsigned char *buffer,*end;
	unsigned int buffersize;
	unsigned char *rp,*wp;
	unsigned int nreaders,nwriters;       
	struct mutex mutex;
	wait_queue_head_t r_wait;			//阻塞读用的等待队列头
	wait_queue_head_t w_wait;			//阻塞写用的等待队列头
	struct fasync_struct *async_queue;	//异步结构体指针，用于读
}crystal_cdev_devp;

/**判断存储空间是否为空
*  return free bytes counts
*/
ssize_t spacefree(struct crystal_cdev_dev *p)
{
	CRYSTAL_DEBUG("%s\n",__func__);
	if( p->rp == p->wp )
		return p->buffersize-1;
	return ((p->rp-p->wp+p->buffersize)%p->buffersize)-1;
}

/*文件打开函数*/
static int crystal_cdev_open(struct inode *inode, struct file *filp)
{
	struct crystal_cdev_dev *dev;
	CRYSTAL_DEBUG("%s\n",__func__);
	dev = container_of(inode->i_cdev,struct crystal_cdev_dev,cdev);
	filp->private_data = dev;
	if( filp->f_mode &FMODE_READ )
		dev->nreaders++;
	if( filp->f_mode &FMODE_WRITE )
		dev->nwriters++;
	return 0;
}

/*文件写函数*/
static ssize_t crystal_cdev_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct crystal_cdev_dev *dev = filp->private_data;
	int ret;
	DECLARE_WAITQUEUE(wait, current);
	CRYSTAL_DEBUG("%s\n",__func__);
	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->w_wait, &wait);

	while (spacefree(dev)==0) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&dev->mutex);
		schedule();
		if (signal_pending(current)){
			ret = -ERESTARTSYS;
			goto out2;
		}
		mutex_lock(&dev->mutex);
	}

	count = min(count,(size_t)spacefree(dev));
		if(dev->wp >= dev->rp )
			count = min(count,(size_t)(dev->end-dev->wp));
		else
			count = min(count,(size_t)(dev->rp-dev->wp-1));

	if (copy_from_user(dev->wp, buf, count)) {
		ret = -EFAULT;
		goto out;
	} else {
		dev->wp += count;
		if(dev->wp==dev->end)
		dev->wp=dev->buffer;
		CRYSTAL_DEBUG("write %zu bytes(s),current_wp_addr = 0x%p\n", count, dev->wp);
		wake_up_interruptible(&dev->r_wait);
		if (dev->async_queue) {
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
			CRYSTAL_DEBUG("kill SIGIO by something has written\n");
		}
		ret = count;
	}

out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->w_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

/*文件读函数*/
static ssize_t crystal_cdev_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *ppos)
{
	int ret;
	struct crystal_cdev_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);
	CRYSTAL_DEBUG("%s\n",__func__);
	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->r_wait, &wait);

	while (dev->rp == dev->wp) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&dev->mutex);
		schedule();
			if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			goto out2;
		}
		mutex_lock(&dev->mutex);
	}

	if( dev->wp > dev->rp )
		count = min(count,(size_t)(dev->wp-dev->rp));
	else
		count = min(count,(size_t)(dev->end-dev->rp));

	if (copy_to_user(buf, dev->rp, count)) {
		ret = -EFAULT;
		goto out;
	} else {
		dev->rp += count;
		if(dev->rp == dev->end)
 			dev->rp = dev->buffer;
		CRYSTAL_DEBUG("read %zu bytes(s),current_rp_addr = 0x%p\n", count, dev->rp);
 		wake_up_interruptible(&dev->w_wait);
		ret = count;
	}

out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->r_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

/* ioctl设备控制函数 */
static long crystal_cdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct crystal_cdev_dev *dev = filp->private_data;
	CRYSTAL_DEBUG("%s\n",__func__);
	switch (cmd)
	{
		case MEM_CLEAR:
			mutex_lock(&dev->mutex);
			dev->rp = crystal_cdev_devp.buffer;
			dev->wp = crystal_cdev_devp.buffer;
			dev->end = crystal_cdev_devp.buffer+crystal_cdev_devp.buffersize;
			memset(dev->buffer, 0, crystal_cdev_devp.buffersize);      
			mutex_unlock(&dev->mutex);
			CRYSTAL_DEBUG("crystal_cdev is set to zero\n");
			break;
		default:
			return  - EINVAL;
	}
	return 0;
}

/* poll设备控制函数 */
static unsigned int crystal_cdev_poll(struct file *filp, poll_table * wait)
{
	unsigned int mask = 0;
	struct crystal_cdev_dev *dev = filp->private_data;
	CRYSTAL_DEBUG("%s\n",__func__);
	mutex_lock(&dev->mutex);
	poll_wait(filp, &dev->r_wait, wait);
	poll_wait(filp, &dev->w_wait, wait);
	if(dev->rp!=dev->wp) {
		mask |= POLLIN | POLLRDNORM;
	}
	if(spacefree(dev)) {
		mask |= POLLOUT | POLLWRNORM;
	}
	mutex_unlock(&dev->mutex);
	return mask;
}

/* 异步通知函数 */
static int crystal_cdev_fasync(int fd, struct file *filp, int mode)
{
	struct crystal_cdev_dev *dev = filp->private_data;
	CRYSTAL_DEBUG("%s\n",__func__);
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

/*文件释放函数*/
static int crystal_cdev_release(struct inode *inode, struct file *filp)
{
	CRYSTAL_DEBUG("%s\n",__func__);
	crystal_cdev_fasync(-1, filp, 0);
	return 0;
}

/*文件操作结构体*/
static const struct file_operations crystal_cdev_fops = {
	.owner = THIS_MODULE,
	.read = crystal_cdev_read,
	.write = crystal_cdev_write,
	.unlocked_ioctl = crystal_cdev_ioctl,
	.poll = crystal_cdev_poll,
	.fasync = crystal_cdev_fasync,
	.open = crystal_cdev_open,
	.release = crystal_cdev_release,
};

/*初始化并注册cdev*/
static void crystal_cdev_setup_cdev(struct crystal_cdev_dev *dev, int index)
{
	int err, devno;
	devno = MKDEV(crystal_cdev_major, index);
	CRYSTAL_DEBUG("%s\n",__func__);
	cdev_init(&dev->cdev, &crystal_cdev_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &crystal_cdev_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		CRYSTAL_ERROR("Error %d adding cdev%d", err, index);
}

/*proc文件读写,区分内核版本*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int crystal_cdev_proc_read(char *page, char **start, off_t off, int count,
		int *eof, void *data)
{
	char *ptr = NULL;
	char *proc_page;
	int len = -1;
	CRYSTAL_DEBUG("%s\n",__func__);
	proc_page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!proc_page) {
		return -ENOMEM;
	}
	ptr = proc_page;
	ptr += sprintf(ptr, "================== Hello =================\n");
	ptr += sprintf(ptr, "===== Wellcom to Crystal's Cdev World=====\n");
	ptr += sprintf(ptr, "==========================================\n");
	ptr += sprintf(ptr, "========= some information below =========\n");
	ptr += sprintf(ptr, "= crystal_cdev_debug = %d\n",crystal_cdev_debug);
	ptr += sprintf(ptr, "= buffer's size = %d\n",crystal_cdev_devp.buffersize);
	len = sprintf(page, "%s\n", proc_page);

	kfree(proc_page);
	return len;
}

static int crystal_cdev_proc_write(struct file *file, const char *buffer,
		unsigned long count, void *data)
{
	int ret = 0;
	char temp[25] = { 0 };	//for store special format cmd
	char mode_str[15] = { 0 };
	unsigned int mode;
	CRYSTAL_DEBUG("%s\n",__func__);
	if (count > 25) {
		return -EFAULT;
	}
	if (copy_from_user(temp, buffer, count)) {
		return -EFAULT;
	}
	temp[count] = '\0';

	ret = sscanf(temp, "%s %d", (char *)&mode_str, &mode);
	CRYSTAL_DEBUG("mode_str=%s mode=%d\n",mode_str,mode);

	if (strcmp(mode_str, "debug") == 0) {
		crystal_cdev_debug = 1;
	}
	else if (strcmp(mode_str, "user") == 0) {
		crystal_cdev_debug = 0;
	}
	else{
		crystal_cdev_debug = 0;
	}
	CRYSTAL_DEBUG("crystal_cdev_debug= %d\n",crystal_cdev_debug);
	return count;
}
#else
static ssize_t crystal_cdev_read_proc(struct file *file, char *buffer,size_t count, loff_t *ppos)
{
	char *ptr = NULL;
	char *proc_page;
	int len, err = -1;
	CRYSTAL_DEBUG("%s\n",__func__);
	proc_page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!proc_page) {
		return -ENOMEM;
	}
	ptr = proc_page;
	ptr += sprintf(ptr, "================== Hello =================\n");
	ptr += sprintf(ptr, "===== Wellcom to Crystal's Cdev World=====\n");
	ptr += sprintf(ptr, "==========================================\n");
	ptr += sprintf(ptr, "========= some information below =========\n");
	ptr += sprintf(ptr, "= crystal_cdev_debug = %d\n",crystal_cdev_debug);
	ptr += sprintf(ptr, "= buffer's size = %d\n",crystal_cdev_devp.buffersize);
	len = ptr - proc_page;
	if (*ppos >= len) {
		kfree(proc_page);
		return 0;
	}
	err = copy_to_user(buffer, (char *)proc_page, len);
	*ppos += len;
	if (err) {
		kfree(proc_page);
		return err;
	}
	kfree(proc_page);
	return len;
}

static ssize_t crystal_cdev_write_proc(struct file *file, const char *buffer,
					size_t count, loff_t *ppos)
{
	int ret = 0;
	char temp[25] = { 0 };	//for store special format cmd
	char mode_str[15] = { 0 };
	unsigned int mode;
	CRYSTAL_DEBUG("%s\n",__func__);
	if (count > 25) {
		return -EFAULT;
	}
	if (copy_from_user(temp, buffer, count)) {
		return -EFAULT;
	}
	temp[count] = '\0';

	ret = sscanf(temp, "%s %d", (char *)&mode_str, &mode);
	CRYSTAL_DEBUG("mode_str=%s mode=%d\n",mode_str,mode);

	if (strcmp(mode_str, "debug") == 0) {
		crystal_cdev_debug = 1;
	}
	else if (strcmp(mode_str, "user") == 0) {
		crystal_cdev_debug = 0;
	}
	else{
		crystal_cdev_debug = 0;
	}
	CRYSTAL_DEBUG("crystal_cdev_debug= %d\n",crystal_cdev_debug);
	return count;
}

static const struct file_operations crystal_cdev_proc_fops = {
  	.owner = THIS_MODULE,
	.write = crystal_cdev_write_proc,
	.read = crystal_cdev_read_proc
};
#endif

/*proc文件系统创建*/
static int crystal_cdev_proc_init(void)
{
	CRYSTAL_DEBUG("%s\n",__func__);
	crystal_cdev_dir = proc_mkdir("crystal_cdev", NULL);
	if (crystal_cdev_dir) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
		crystal_cdev_entry = create_proc_entry("crystal_cdev_info", 0666, crystal_cdev_dir);
		if (crystal_cdev_entry) {
			crystal_cdev_entry->nlink = 1;
			crystal_cdev_entry->data = &variable;
			crystal_cdev_entry->read_proc = crystal_cdev_proc_read;
			crystal_cdev_entry->write_proc = crystal_cdev_proc_write;
			return 0;
		}
#else
		crystal_cdev_entry = proc_create_data("crystal_cdev_info",0666,crystal_cdev_dir, &crystal_cdev_proc_fops, &variable);
		if (crystal_cdev_entry){
			return 0;
		}
#endif
	}
	return -ENOMEM;
}

/*设备驱动模块加载函数*/
static int __init crystal_cdev_init(void)
{
	int ret;
	dev_t devno = MKDEV(crystal_cdev_major, 0);
	CRYSTAL_DEBUG("%s\n",__func__);
	if (crystal_cdev_major)	//申请设备号
		ret = register_chrdev_region(devno, 1, DEVICE_NAME);
	else{	//动态申请设备号
		ret = alloc_chrdev_region(&devno, 0, 1, DEVICE_NAME);
		crystal_cdev_major = MAJOR(devno);
	}  
	if (ret < 0)
		return ret;

	crystal_cdev_devp.buffersize = buffersize;
	crystal_cdev_devp.buffer = kmalloc(crystal_cdev_devp.buffersize, GFP_KERNEL);
	if ( !crystal_cdev_devp.buffer ) 
	{
		ret =  - ENOMEM;
		goto fail_malloc;
	}
	CRYSTAL_DEBUG("begin = 0x%p , end = 0x%p\n", crystal_cdev_devp.buffer,crystal_cdev_devp.buffer+crystal_cdev_devp.buffersize);
	memset(crystal_cdev_devp.buffer, 0, crystal_cdev_devp.buffersize);

	crystalclass = class_create(THIS_MODULE,DEVICE_NAME);
	crystal_cdev_setup_cdev(&crystal_cdev_devp, 0);
	/*创建设备节点*/
	device_create(crystalclass,NULL,MKDEV(crystal_cdev_major, 0),NULL,DEVICE_NAME);
 	/*创建 proc 文件系统 */ 
	crystal_cdev_proc_init();
	/*数据初始化*/
	crystal_cdev_devp.rp = crystal_cdev_devp.buffer;
	crystal_cdev_devp.wp = crystal_cdev_devp.buffer;
	crystal_cdev_devp.end = crystal_cdev_devp.buffer+crystal_cdev_devp.buffersize;
	crystal_cdev_devp.nreaders = 0;
	crystal_cdev_devp.nwriters = 0;
	mutex_init(&crystal_cdev_devp.mutex);
	init_waitqueue_head(&crystal_cdev_devp.r_wait);
	init_waitqueue_head(&crystal_cdev_devp.w_wait);
	return 0;

 fail_malloc: 
	unregister_chrdev_region(devno, 1);
	return ret;
}

/*模块卸载函数*/
void crystal_cdev_exit(void)
{
	cdev_del(&crystal_cdev_devp.cdev);							//注销cdev
	remove_proc_entry("crystal_cdev_info", crystal_cdev_dir);
	remove_proc_entry("crystal_cdev", NULL);
	device_destroy(crystalclass,MKDEV(crystal_cdev_major, 0));	//摧毁设备节点函数
	class_destroy(crystalclass);								//释放设备class
	kfree(crystal_cdev_devp.buffer);							//释放全局内存空间
	unregister_chrdev_region(MKDEV(crystal_cdev_major, 0), 1);	//释放设备号
}

module_init(crystal_cdev_init);
module_exit(crystal_cdev_exit);

MODULE_AUTHOR("Crystal Shen <crystal.shen@163.com>");
MODULE_DESCRIPTION("A char device drivers for Crystal's study");
MODULE_VERSION("V1.0");
MODULE_ALIAS("Hello Crystal");
MODULE_LICENSE("Dual BSD/GPL");
