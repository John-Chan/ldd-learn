#ifndef     SSXA_PCI_CDEV_FILE_OPS_H
#define     SSXA_PCI_CDEV_FILE_OPS_H
#include <linux/fs.h>


/**
 * This function is called when the device node is opened
 *
 */
int fop_open(struct inode *inode, struct file *file);

/**
 * This function is called when the device node is closed
 *
 */
int fop_release(struct inode *inode, struct file *file);

/**
 * This function is called when the device node is read
 *
 */
ssize_t fop_read(struct file *file,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset);

/**
 * This function is called when the device node is read
 *
 */
ssize_t fop_write(struct file *filp, const char *buffer, size_t len, loff_t * off) ;

#endif//!SSXA_PCI_CDEV_FILE_OPS_H
