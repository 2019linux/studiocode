
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


MODULE_LICENSE("Dual BSD/GPL");
int buf_size =  1024*1024*4;

int *bufp = NULL;
struct kmem_cache       *test_buf_cache;

static void __exit scull_exit(void)
{
	//kfree(bufp);
	kmem_cache_free(test_buf_cache, bufp);
	 kmem_cache_destroy(test_buf_cache);

}

static int __init scull_init(void)
{
	//bufp = kmalloc(buf_size+1, GFP_KERNEL);
	test_buf_cache = kmem_cache_create("hello", buf_size, 512, 0, NULL);
	bufp = kmem_cache_alloc(test_buf_cache, GFP_KERNEL);
	if(!bufp) 
		printk("[1]kmalloc buf fail\n");
	else
		printk("[1]kmalloc buf sucess !!\n");
	return 0;
}



module_init(scull_init);
module_exit(scull_exit);
