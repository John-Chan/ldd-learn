#ifndef     LDD_CHAR_DEVICE_1_H
#define     LDD_CHAR_DEVICE_1_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>



int     init_moudle(void);
void    cleanup_module(void);
int  device_open(struct inode *,struct file *);
int  device_release(struct inode*, struct file *);

#endif//!LDD_CHAR_DEVICE_1_H


