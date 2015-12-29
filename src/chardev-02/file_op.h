#ifndef     LDD_CHAR_DEVICE_FILE_OPS_H
#define     LDD_CHAR_DEVICE_FILE_OPS_H
#include <linux/fs.h>

ssize_t  device_read(struct file *,char *,size_t ,loff_t *);
ssize_t  device_write(struct file *,const char *, size_t , loff_t *);

#endif//!LDD_CHAR_DEVICE_FILE_OPS_H
