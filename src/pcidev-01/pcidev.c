#include "shared.h"
#include "common.h"

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/cdev.h>		/* for cdev_ */
#include <asm/uaccess.h>    /* for put_user */

 
#define BAR_IO			SSA10A_BAR_IO
#define BAR_MEM			SSA10A_BAR_MEM

MODULE_DESCRIPTION("SSX10A PCI  driver");
MODULE_AUTHOR("john (cpp.cheen@gmail.com)");
MODULE_LICENSE("GPL");

/**********************************************************************************************************************

From Documentation/PCI/pci.txt, the following is the general flow when writing initialization code for a PCI device:
1 Register the device driver and find the device.
2 Enable the device.
3 Request MMIO/IOP resources
4 Set the DMA mask size (for both coherent and streaming DMA)
5 Allocate and initialize shared control data.
6 Access device configuration space (if needed)
7 Register IRQ handler.
8 Initialize non-PCI (i.e. LAN/SCSI/etc parts of the chip)
9 Enable DMA/processing engines.

**********************************************************************************************************************/
/**
 * This table holds the list of (VendorID,DeviceID) supported by this driver
 *
 */ 
static struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(SSX10A_VENDER_ID, SSA10A_DEVICE_ID), },
	{ 0, }
};

/**
 * This macro ...
 *
 */
MODULE_DEVICE_TABLE(pci, pci_ids);

static dev_t devno;
static int major;

/**
 *  This structure is used to link a pci_dev to a cdev 
 *
 */
struct pci_cdev_pair {
	int minor;
	struct pci_dev *pci_dev;
	struct cdev *cdev;
};

static struct pci_cdev_pair pci_cdev[MAX_DEVICE];

/* this function initialize the table with all struct pci_cdev */
static void pci_cdev_init(struct pci_cdev_pair pci_cdev[], int size, int first_minor)
{
	int i;

	for(i=0; i<size; i++) {
		pci_cdev[i].minor   = first_minor++;
		pci_cdev[i].pci_dev = NULL;
		pci_cdev[i].cdev    = NULL;
	}
}

/*
	-1 	=> failed
	 others => succes
*/
static int pci_cdev_add(struct pci_cdev_pair pci_cdev[], int size, struct pci_dev *pdev)
{
	int i, res = -1;

	for(i=0; i<size; i++) {
		if (pci_cdev[i].pci_dev == NULL) {
			pci_cdev[i].pci_dev = pdev;
			res = pci_cdev[i].minor;
			break;
		}
	}
	
	return res;
}

static void pci_cdev_del(struct pci_cdev_pair pci_cdev[], int size, struct pci_dev *pdev)
{
	int i;

	for(i=0; i<size; i++) {
		if (pci_cdev[i].pci_dev == pdev) {
			pci_cdev[i].pci_dev = NULL;
		}
	}
}

static struct pci_dev *pci_cdev_search_pci_dev(struct pci_cdev_pair pci_cdev[], int size, int minor)
{
	int i;
	struct pci_dev *pdev = NULL;

	for(i=0; i<size; i++) {
		if (pci_cdev[i].minor == minor) {
			pdev = pci_cdev[i].pci_dev;
			break;
		}
	}

	return pdev;	
}

static struct cdev *pci_cdev_search_cdev(struct pci_cdev_pair pci_cdev[], int size, int minor)
{
	int i;
	struct cdev *cdev = NULL;

	for(i=0; i<size; i++) {
		if (pci_cdev[i].minor == minor) {
			cdev = pci_cdev[i].cdev;
			break;
		}
	}

	return cdev;	
}

/* 
 	-1 	=> not found
	others	=> found
*/
static int pci_cdev_search_minor(struct pci_cdev_pair pci_cdev[], 
		int size, struct pci_dev *pdev)
{
	int i, minor = -1;

	for(i=0; i<size; i++) {
		if (pci_cdev[i].pci_dev == pdev) {
			minor = pci_cdev[i].minor;
			break;
		}
	}

	return minor;
}



/**
 * This function is called when the device node is opened
 *
 */
static int pci_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);
	file->private_data = (void *)pci_cdev_search_pci_dev(pci_cdev, MAX_DEVICE, minor);
	return 0;
}

/**
 * This function is called when the device node is closed
 *
 */
static int pci_release(struct inode *inode, struct file *file)
{
	return 0;
}

/**
 * This function is called when the device node is read
 *
 */
static ssize_t pci_read(struct file *file,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
	int byte_read = 0;
	unsigned char value;
	struct pci_dev *pdev = (struct pci_dev *)file->private_data;
	unsigned long pci_io_addr = 0;

	pci_io_addr = pci_resource_start(pdev,BAR_IO);

	while (byte_read < length) {
		/* read a byte from the input */
		value = inb(pci_io_addr + 1);

		/* write the value in the user buffer */
		put_user(value, &buffer[byte_read]);

		byte_read++;
	}

	return byte_read;
}

/**
 * This function is called when the device node is read
 *
 */
static ssize_t pci_write(struct file *filp, const char *buffer, size_t len, loff_t * off) {
	int i;
	unsigned char value;
	struct pci_dev *pdev = (struct pci_dev *)filp->private_data;
	unsigned long pci_io_addr = 0;

	pci_io_addr = pci_resource_start(pdev,BAR_IO);
	
	for(i=0; i<len; i++) {
		/* read value on the buffer */
		value = (unsigned char)buffer[i];

		/* write data to the device */
		outb(pci_io_addr+2, value);
	}

	return len;
}

/**
 * This structure holds informations about the pci node
 *
 */
static struct file_operations pci_ops = {
	.owner		= THIS_MODULE,
	.read 		= pci_read,
	.write 		= pci_write,
	.open 		= pci_open,
	.release 	= pci_release
};


/**
 * This function is called when a new pci device is associated with a driver
 *
 * return: 0 => this driver don't handle this device
 *         1 => this driver handle this device
 *
 */
static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int ret, minor;
	struct cdev *cdev;
	dev_t devno;

	/* add this pci device in pci_cdev */
	if ((minor = pci_cdev_add(pci_cdev, MAX_DEVICE, dev)) < 0)
		goto error;

	/* compute major/minor number */
	devno = MKDEV(major, minor);

	/* allocate struct cdev */
	cdev = cdev_alloc();

	/* initialise struct cdev */
	cdev_init(cdev, &pci_ops);
	cdev->owner = THIS_MODULE;

	/* register cdev */
	ret = cdev_add(cdev, devno, 1);
	if (ret < 0) {
		dev_err(&(dev->dev), "Can't register character device\n");
		goto error;
	}
	pci_cdev[minor].cdev = cdev;

	dev_info(&(dev->dev), "%s The major device number is %d (%d).\n",
	       "Registeration is a success", MAJOR(devno), MINOR(devno));
	dev_info(&(dev->dev), "If you want to talk to the device driver,\n");
	dev_info(&(dev->dev), "you'll have to create a device file. \n");
	dev_info(&(dev->dev), "We suggest you use:\n");
	dev_info(&(dev->dev), "mknod %s c %d %d\n", PCI_DEVICE_NAME, MAJOR(devno), MINOR(devno));
	dev_info(&(dev->dev), "The device file name is important, because\n");
	dev_info(&(dev->dev), "the ioctl program assumes that's the\n");
	dev_info(&(dev->dev), "file you'll use.\n");

	/* enable the device */
	pci_enable_device(dev);

	/* 'alloc' IO to talk with the card */
	if (pci_request_region(dev, SSA10A_BAR_IO, "IO-pci") == 0) {
		dev_err(&(dev->dev), "Can't request BAR2\n");
		cdev_del(cdev);
		goto error;
	}

	/* check that SSA10A_BAR_IO is *really* IO region */
	if ((pci_resource_flags(dev, SSA10A_BAR_IO) & IORESOURCE_IO) != IORESOURCE_IO) {
		dev_err(&(dev->dev), "SSA10A_BAR_IO isn't an IO region\n");
		cdev_del(cdev);
		goto error;
	}

    /////////////////////////////////////////////////////////////////
    resource_size_t start, len, end;
    unsigned long flags;

    start = pci_resource_start(dev, SSA10A_BAR_IO);
    len  = pci_resource_len(dev, SSA10A_BAR_IO);
    end = pci_resource_end(dev, SSA10A_BAR_IO);
    flags = pci_resource_flags(dev, SSA10A_BAR_IO);
    
    printk(KERN_ALERT DEBUG_TAG "SSA10A PCI BAR %d,start=%08X,end=%08X,len=%08X,flags=%08X\n",
            SSA10A_BAR_IO,start,end,len,flags);
    /////////////////////////////////////////////////////////////////
	return 1;

error:
	return 0;
}

/**
 * This function is called when the driver is removed
 *
 */
static void pci_remove(struct pci_dev *dev)
{
	int minor;
	struct cdev *cdev;

	/* remove associated cdev */
	minor = pci_cdev_search_minor(pci_cdev, MAX_DEVICE, dev);
	cdev = pci_cdev_search_cdev(pci_cdev, MAX_DEVICE, minor);
	if (cdev != NULL) 
		cdev_del(cdev);
		
	/* remove this device from pci_cdev */
	pci_cdev_del(pci_cdev, MAX_DEVICE, dev);

	/* release the IO region */
	pci_release_region(dev, SSA10A_BAR_IO);
}

/**
 * This structure holds informations about the pci driver
 *
 */
static struct pci_driver pci_driver = {
	.name 		= "pci",
	.id_table 	= pci_ids,
	.probe 		= pci_probe,
	.remove 	= pci_remove,
};

/////////////////////////////////////////////////////////////////////////////////////////////

//#define     PCI_DEVICE_NAME     "ssxa-pci"    
//#define     CHR_DEVICE_NAME     "ssxa-cdev"     

/*
* this function is called when the module is loaded
*/
static int     __init   module_load(void)
{
    printk(KERN_ALERT DEBUG_TAG "module_load\n");
    
    int ret;
    // allocate (several) major number ,start from 0
    // name will display in /proc/devices
    ret= alloc_chrdev_region(&devno,0,MAX_DEVICE,CHR_DEVICE_NAME);
    if (ret < 0) {
		printk(KERN_ERR DEBUG_TAG "Can't get major\n");
		return ret;
	}
    
	/* get major number and save it in major */
	major = MAJOR(devno);
    
	/* initialise pci_cdev */
	pci_cdev_init(pci_cdev, MAX_DEVICE, MINOR(devno));
    
    /* register pci driver */
	ret = pci_register_driver(&pci_driver);
	if (ret < 0) {
		/* free major/minor number */
		unregister_chrdev_region(devno, 1);

		printk(KERN_ERR DEBUG_TAG "pci-driver: can't register pci driver\n");
		return ret;
	}
    return NO_ERROR;
    ////////////////////////////////////////////////////////////////
    
    /*
    major= register_chrdev(0,CHR_DEVICE_NAME,&fops);
    if(major < 0){
        printk(KERN_ALERT  DEBUG_TAG "register_chrdev fail:%d\n",major);
        return major;
    }
    ret=poc_entry_open(PROC_FS_ENTRY_NAME);
    if(0!=ret){      
        printk(KERN_ALERT  DEBUG_TAG "poc_entry_open fail:%d\n",ret);
        return ret;
    }
    
    
    printk(KERN_ALERT  DEBUG_TAG "Assigned major numner %d \n",major);
    printk(KERN_ALERT  DEBUG_TAG "to talk to the driver,create a device file with:\n");
    printk(KERN_ALERT  DEBUG_TAG "mknod /dev/%s c %d 0\n",DEVICE_NAME,major);
    printk(KERN_ALERT  DEBUG_TAG "and try cat echo to the device file\n" );
    printk(KERN_ALERT  DEBUG_TAG "to remove:\n");
    printk(KERN_ALERT  DEBUG_TAG "rm -f /dev/%s\n",DEVICE_NAME);
    //printk_flush();
    
    return NO_ERROR;
    */
 
}

/*
* this function is called when the module is unloaded
*
*/
static void    __exit   module_unload(void)
{
    int i;
    /* unregister pci driver */
    pci_unregister_driver(&pci_driver);
    /* unregister character device */
    for(i=0;i<MAX_DEVICE;++i){
        if(pci_cdev[i].pci_cdev != NULL){
            cdev_del(pci_cdev[i].cdev);
        }
    }
    
    
	/* free major/minor number */
	unregister_chrdev_region(devno, MAX_DEVICE);
    
    printk(KERN_ALERT  DEBUG_TAG "module_unload\n");
}


module_init(module_load); 
module_exit(module_unload); 