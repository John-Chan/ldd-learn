#include "file_op.h"
#include "shared.h"
#include <linux/uaccess.h>


ssize_t  device_read(struct file * fp,char * buffer,size_t size,loff_t * offset)
{
    // bytes actually written to the user buffer
    int bytes_read=0;
    
    if(*msg_ptr == 0){
        return 0;
    }
    while(size && *msg_ptr != 0){
        // the buffer is in the user data segment,not the 
        // kernel segment so "*" assignment won't work.
        // we have to use put_user to copy data form the kernrl
        // data segment to the user data segment
        
        put_user(*(msg_ptr++),buffer++);
        --size;
        ++bytes_read;
    }
    return bytes_read;
}

ssize_t  device_write(struct file * fp,const char * ptr, size_t size, loff_t * offset)
{
    printk(KERN_ALERT "this operation is not supported yet");
    return -EINVAL;
}