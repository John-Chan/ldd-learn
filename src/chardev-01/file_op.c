
#include "file_op.h"
#include <linux/uaccess.h>


ssize_t  device_read(struct file *,char *,size_t ,loff_t *)
{
    return -1;
}

ssize_t  device_write(struct file *,const char *, size_t , loff_t *)
{
    //
    return -1;
}