#include "chardev.h"
#include "file_op.h"

static int major;
static int has_open=0;
static char msg_buff[BUFFER_SIZE];
static char* msg_ptr;

static struct file_operations fops={
    .read       =   device_read,
    .write      =   device_write,
    .open       =   device_open,
    .release    =   device_release
};


MODULE_DESCRIPTION("A simple PCI driver");
MODULE_AUTHOR("trem (tremyfr@yahoo.fr)");
MODULE_LICENSE("GPL");

/*
* this function is called when the module is loaded
*/
int     init_moudle(void)
{
    printk(KERN_INFO "init_moudle");
    return NO_ERROR;
    
    (void)fops;
    (void)major;
    (void)has_open;
    (void)msg_buff;
    (void)msg_ptr;
}

/*
* this function is called when the module is unloaded
*
*/
void    cleanup_module(void)
{
    printk(KERN_INFO "cleanup_module");
}


int  device_open(struct inode * nod,struct file * fp)
{
    //
    return -1;
}
int  device_release(struct inode* nod, struct file * fp)
{
    //
    return -1;
}
