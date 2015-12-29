#ifndef     LDD_CHAR_DEVICE_1_H
#define     LDD_CHAR_DEVICE_1_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>


/*
 *
 */
#define     MAX_DEVICE      32
#define     DEVICE_NAME     "CHAR-DEV"      /* will show in '/proc/devices' */
#define     BUFFER_SIZE     32
#define     NO_ERROR        0


int     init_moudle(void);
void    cleanup_module(void);
static int  device_open(struct inode *,struct file *);
static int  device_release(struct inode*, struct file *);

#endif//!LDD_CHAR_DEVICE_1_H


