
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>  
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <linux/uaccess.h>

#include "scull.h"

MODULE_LICENSE("Dual BSD/GPL");

static unsigned int scull_major;
static struct class *scullclass;

int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

static struct scull_dev  *scull_devices;



int scull_trim(struct scull_dev *dev)
{
	struct scull_qset *next,*dptr;
	int qset = dev->qset;
	int i;
	
	for(dptr = dev->data;dptr;dptr=next){
		if(dptr->data){
			for(i = 0; i < qset; i++)
				kfree(dptr->data[i]);
			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		kfree(dptr);
	}
	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset;
	dev->data = NULL;
	return 0;
}

int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev; /* device information */
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev; /* for other methods */

	/* now trim to 0 the length of the device if open was write-only */
	if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
		scull_trim(dev); /* ignore errors */
	}
	return 0; /* success */
}

int scull_release(struct inode *inode, struct file *filp)
{
	 return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev,int n)
{
	struct scull_qset *qs = dev->data;
	if(!qs){
		qs = dev->data = kmalloc(sizeof(struct scull_qset),GFP_KERNEL);
		if(qs == NULL)
			return NULL;
		memset(qs,0,sizeof(struct scull_qset));
	}
	while(n--){
		if(!qs->next){
			qs->next = kmalloc(sizeof(struct scull_qset),GFP_KERNEL);
			if(qs->next == NULL)
				return NULL;
			memset(qs->next,0,sizeof(struct scull_qset));
		}
		qs = qs->next;
		continue;
	}
	return qs;
}

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
        struct scull_dev *dev = filp->private_data;
        struct scull_qset *dptr; /* the first listitem */
        int quantum = dev->quantum, qset = dev->qset;
        int itemsize = quantum * qset; /* how many bytes in the listitem */
        int item, s_pos, q_pos, rest;
        ssize_t retval = 0;

//        if (down_interruptible(&dev->sem))
//                return -ERESTARTSYS;
        if (*f_pos >= dev->size)
                goto out;
        if (*f_pos + count > dev->size)
                count = dev->size - *f_pos;

        /* find listitem, qset index, and offset in the quantum */
        item = (long)*f_pos / itemsize;
        rest = (long)*f_pos % itemsize;
        s_pos = rest / quantum;
        q_pos = rest % quantum;

        /* follow the list up to the right position (defined elsewhere) */
        dptr = scull_follow(dev, item);
        if (dptr == NULL || !dptr->data || ! dptr->data[s_pos])
                goto out; /* don't fill holes */

        /* read only up to the end of this quantum */
        if (count > quantum - q_pos)
                count = quantum - q_pos;

        if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count))
        {
                retval = -EFAULT;
                goto out;

        }
        *f_pos += count;
        retval = count;

out:
//        up(&dev->sem);
        return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
        struct scull_dev *dev = filp->private_data;
        struct scull_qset *dptr;
        int quantum = dev->quantum, qset = dev->qset;
        int itemsize = quantum * qset;
        int item, s_pos, q_pos, rest;
        ssize_t retval = -ENOMEM; /* value used in "goto out" statements */
//        if (down_interruptible(&dev->sem))
//                return -ERESTARTSYS;

        /* find listitem, qset index and offset in the quantum */
        item = (long)*f_pos / itemsize;
        rest = (long)*f_pos % itemsize;
        s_pos = rest / quantum;
        q_pos = rest % quantum;
        /* follow the list up to the right position */
        dptr = scull_follow(dev, item);
        if (dptr == NULL)
                goto out;
        if (!dptr->data)
        {
                dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
                if (!dptr->data)
                        goto out;
                memset(dptr->data, 0, qset * sizeof(char *));
        }
        if (!dptr->data[s_pos])
        {
                dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
                if (!dptr->data[s_pos])

                        goto out;
        }
        /* write only up to the end of this quantum */
        if (count > quantum - q_pos)

                count = quantum - q_pos;
        if (copy_from_user(dptr->data[s_pos]+q_pos, buf, count))
        {
                retval = -EFAULT;
                goto out;

        }
        *f_pos += count;
        retval = count;

        /* update the size */
        if (dev->size < *f_pos)
                dev->size = *f_pos;

out:
//        up(&dev->sem);
        return retval;

}


static struct file_operations scull_fops = {
	 .owner =  THIS_MODULE, 
	 //.llseek =  scull_llseek, 
	 .read =  scull_read, 
	 .write =  scull_write, 
	 //.ioctl =  scull_ioctl, 
	.open =  scull_open, 
	.release =  scull_release,  
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

	device_destroy(scullclass,devno);
	class_destroy(scullclass);
	if(scull_devices){
		scull_trim(scull_devices + 1);				
		cdev_del(&scull_devices->cdev);
	}

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
