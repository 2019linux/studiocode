#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual BSD/GPL");

static unsigned int scull_major;
static struct class *scullclass;

struct scull_dev {                                                        
	  struct cdev cdev;                     
	  unsigned char *memdata;        
};

static struct scull_dev  *scull_devices;

static struct file_operations scull_fops = {
	 .owner =  THIS_MODULE, 
	 //.llseek =  scull_llseek, 
	// .read =  scull_read, 
	// .write =  scull_write, 
	 //.ioctl =  scull_ioctl, 
	// .open =  scull_open, 
	 //.release =  scull_release,  
};

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
	int err, devno = MKDEV(scull_major, index);

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding scull%d", err, index);
} 



static void __exit scull_exit(void)
{

	dev_t devno = MKDEV(scull_major,0);

	cdev_del(&scull_devices->cdev);

	device_destroy(scullclass,devno);
	class_destroy(scullclass);
        kfree(scull_devices); 	
	unregister_chrdev_region(devno, 1);
}

static int __init scull_init(void)
{

	int result;
	dev_t dev = 0;

	printk(KERN_INFO "The process is \"%s\" (pid %i)\n", current->comm, current->pid);

	result = alloc_chrdev_region(&dev, 0, 1, "scull");
	if (result)
		return result;

	scull_major = MAJOR(dev);
		
	scull_devices = kmalloc(sizeof(struct scull_dev),GFP_KERNEL);
	if(!scull_devices){
		result = -ENOMEM;
		goto fail;
	}
	
	memset(scull_devices,0, sizeof(struct scull_dev));
	printk(KERN_ALERT "The length of scull_dev is %ld",sizeof(struct scull_dev));

	scull_setup_cdev(scull_devices, 0);

	scullclass = class_create(THIS_MODULE,"scull");
	dev = MKDEV(scull_major,0);
	device_create(scullclass, NULL, dev, NULL, "scull");
	return 0;
fail:
	scull_exit();
	return result;
}



module_init(scull_init);
module_exit(scull_exit);
