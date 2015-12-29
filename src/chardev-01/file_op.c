
#include "file_op.h"
#include <linux/uaccess.h>


ssize_t  device_read(struct file * fp,char * ptr,size_t size,loff_t * offset)
{
    return -1;
}

ssize_t  device_write(struct file * fp,const char * ptr, size_t size, loff_t * offset)
{
    //
    return -1;
}