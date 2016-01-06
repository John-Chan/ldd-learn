#include "file_op.h"
#include "shared.h"
#include "common.h"
#include "utils.h"
#include <linux/uaccess.h>


/**
 * This function is called when the device node is opened
 *
 */
int fop_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);
	file->private_data = (void *)pci_cdev_search_pci_dev(pci_cdev, MAX_DEVICE, minor);
	return 0;
}

/**
 * This function is called when the device node is closed
 *
 */
int fop_release(struct inode *inode, struct file *file)
{
	return 0;
}

/**
 * This function is called when the device node is read
 *
 */
ssize_t fop_read(struct file *file,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
	int byte_read = 0;

	return byte_read;
}

/**
 * This function is called when the device node is read
 *
 */
ssize_t fop_write(struct file *filp, const char *buffer, size_t len, loff_t * off) {
	len = 0;

	return len;
}
