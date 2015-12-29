#include "chardev.h"
#include "file_op.h"
#include "shared.h"
 

static int major;
static int has_open=0;

static struct file_operations fops={
    .read       =   device_read,
    .write      =   device_write,
    .open       =   device_open,
    .release    =   device_release
};


MODULE_DESCRIPTION("A simple PCI driver");
MODULE_AUTHOR("john (cpp.cheen@gmail.com)");
MODULE_LICENSE("GPL");

/*
* this function is called when the module is loaded
*/
int     init_module(void)
{
    printk(KERN_ALERT DEBUG_TAG "init_moudle\n");
    
    // aquire new major number,reutrn >0 if success
    major= register_chrdev(0,DEVICE_NAME,&fops);
    if(major < 0){
        printk(KERN_ALERT  DEBUG_TAG "register_chrdev fail:%d\n",major);
        return major;
    }
    printk(KERN_ALERT  DEBUG_TAG "Assigned major numner %d \n",major);
    printk(KERN_ALERT  DEBUG_TAG "to talk to the driver,create a device file with:\n");
    printk(KERN_ALERT  DEBUG_TAG "mknod /dev/%s c %d 0\n",DEVICE_NAME,major);
    printk(KERN_ALERT  DEBUG_TAG "and try cat echo to the device file\n" );
    printk(KERN_ALERT  DEBUG_TAG "to remove:\n");
    printk(KERN_ALERT  DEBUG_TAG "rm -f /dev/%s\n",DEVICE_NAME);
    //printk_flush();
    
    return NO_ERROR;
 
}

/*
* this function is called when the module is unloaded
*
*/
void    cleanup_module(void)
{
    printk(KERN_INFO  DEBUG_TAG "cleanup_module\n");
    // after 2.6.12-rc2,unregister_chrdev return void
    /*
    int ret= unregister_chrdev(major,DEVICE_NAME);
    if(ret < 0){
        printk(KERN_ALERT "unregister_chrdev fail:%d",ret);
    }
    */
}

/*
* called when a process try to open the device file like 'cat /dev/myfile'
*/
int  device_open(struct inode * nod,struct file * fp)
{
    static int counter=0;
    if(has_open != 0){
        return -EBUSY;
    }
    ++has_open;
    sprintf(msg_buff,"device already opened %d times\n",++counter);
    msg_ptr=msg_buff;
    // increment module ref-counter
    try_module_get(THIS_MODULE);
    return NO_ERROR;
}

/*
* called when a process close the device file 
*/
int  device_release(struct inode* nod, struct file * fp)
{
    --has_open;
    // decrement the ref-counter,or else once you opened the file, you 
    // will never get rid of the module
    module_put(THIS_MODULE);
    return NO_ERROR;
}
