#include "shared.h"
#include "common.h"
#include "utils.h"
#include "file_op.h"

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/device.h>   /* for class_create  */
#include <linux/cdev.h>		/* for cdev_ */
#include <asm/uaccess.h>    /* for put_user */
#include <asm/byteorder.h>
#include <asm-generic/bug.h>

 


MODULE_DESCRIPTION("SSX10A PCI  driver");
MODULE_AUTHOR("john (cpp.cheen@gmail.com)");
MODULE_LICENSE("GPL");


/**********************************************************************************************************************

helper functions:

**********************************************************************************************************************/


static void    init_driver_contexts(
    int first_minor,
    int count,
    struct driver_context_t* drv_ctx_head)
{
    BUG_ON(count > MAX_DEVICE); 
    memset(drv_ctx_head,0,sizeof(struct driver_context_t) * count );
 
    for(int i=0;i<count;++i){
        drv_ctx_head[i].minor=first_minor++;
        for(int j=0;j<MAX_CHIP_IN_DEVICE;++j){
            drv_ctx_head[i].workers[j].id = j;
        }
    }
}

static void delete_driver_context(struct driver_context_t* drv_ctx)
{
    //
}
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
	{ PCI_DEVICE(SSX10A_VENDER_ID, SSX10A_DEVICE_ID), },
	{ 0, }
};

/**
 * This macro ...
 *
 */
MODULE_DEVICE_TABLE(pci, pci_ids);

static dev_t devno;
static int major;
static struct  driver_context_t   driver_contexts[MAX_DEVICE];
static struct  class*   dev_class;


/**
 * This structure holds informations about the pci node
 *
 */
static struct file_operations cdev_ops = {
	.owner		= THIS_MODULE,
	.read 		= fop_read,
	.write 		= fop_write,
	.open 		= fop_open,
	.release 	= fop_release
};


/**
 * This function is called when a new pci device is associated with a driver
 *
 * return: 0 => this driver don't handle this device
 *         1 => this driver handle this device
 *
 */
 
static int pci_driver_attach(struct pci_dev *dev, const struct pci_device_id *id)
{
    return 0;
    
    
    dev_info(&(dev->dev), "pci_probe <== \n");
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

    /////////////////////////////////////////////////////


    ret = pci_request_regions(dev,"IO-pci");
    if ( ret )
    {  
        printk(KERN_ERR"call pci_request_regions() falied,error = %d\n",ret);
		cdev_del(cdev);
		goto error;
    }
    
//static  unsigned long g_falgs;
//static  unsigned long g_base_port;
//static  unsigned long g_base_port_size;
//static  unsigned long g_phys_addr;
//static  unsigned long g_mem_size;
    g_phys_addr = 0 ;
    g_mem_size = 0; 
    g_base_port_size=0;
    g_base_port=0;
    g_falgs=0;
    
    ret = 0 ;
    
    int bar;
    //pci_sscrypt_dev = pci_sscrypt_dev_allocate(dev);
    for ( bar = 0; bar < 6; bar ++ )
    {
        g_falgs = pci_resource_flags(dev,bar);
        printk(KERN_ALERT DEBUG_TAG "looking up BAR %d,flags = %08X\n",bar,g_falgs);
        if ( g_falgs&IORESOURCE_IO )
        {
            g_base_port =  pci_resource_start(dev,bar);
            g_base_port_size = pci_resource_len(dev,bar);   
            
            g_w_port = g_base_port;
            g_s_port = g_base_port  + 1;
            g_c_port = g_base_port  + 2;
            g_r_port = g_base_port  + 3;  

            //pci_sscrypt_dev->wport = g_base_port;
            //pci_sscrypt_dev->sport = g_base_port  + 1;
            //pci_sscrypt_dev->cport = g_base_port  + 2;
            //pci_sscrypt_dev->rport = g_base_port  + 3;    

            printk(KERN_ALERT DEBUG_TAG "found IORESOURCE_IO ,base_port=%08X,base_port_size=%08X\n",g_base_port,g_base_port_size);
            printk(KERN_ALERT DEBUG_TAG "write data complete signal port %08X\n",g_w_port);
            printk(KERN_ALERT DEBUG_TAG "process complete signal port %08X\n",g_s_port);
            printk(KERN_ALERT DEBUG_TAG "clear interrupt signal port %08X\n",g_c_port);
            printk(KERN_ALERT DEBUG_TAG "reset device port %08X\n",g_r_port);



            pci_cdev.r_port = g_base_port  + 3;
            pci_cdev.c_port = g_base_port  + 2;
            pci_cdev.s_port = g_base_port  + 1;
            pci_cdev.w_port = g_base_port;
        }
        else if ( g_falgs&IORESOURCE_MEM )
        {
            printk(KERN_ALERT DEBUG_TAG "found IORESOURCE_MEM \n");
            g_phys_addr = pci_resource_start(dev,bar);
            g_mem_size  = pci_resource_len(dev,bar);
            printk(KERN_ALERT DEBUG_TAG "g_phys_addr %08X\n",g_phys_addr);
            printk(KERN_ALERT DEBUG_TAG "g_mem_size %08X\n",g_mem_size);
            
            pci_cdev.phys_addr=g_phys_addr;
            pci_cdev.mem_size=g_mem_size;
        }
    }


    dev_info(&(dev->dev), "pci_probe ==> do handle \n");
	return 1;

error:
    dev_info(&(dev->dev), "pci_probe ==> do not handle \n");
	return 0;
}

/**
 * This function is called when the driver is removed
 *
 */
static void pci_driver_detach(struct pci_dev *dev)
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
	pci_release_regions(dev);
}

/**
 * This structure holds informations about the pci driver
 *
 */
static struct pci_driver pci_driver = {
	.name 		= "pci",
	.id_table 	= pci_ids,
	.probe 		= pci_driver_attach,
	.remove 	= pci_driver_detach,
};

/////////////////////////////////////////////////////////////////////////////////////////////

/*
* this function is called when the module is loaded
*/
static int     __init   module_load(void)
{
    printk(KERN_ALERT DEBUG_TAG "module_load\n");
    
    int ret;
    // allocate (several) major number ,start from 0
    // name will display in /proc/devices
    ret= alloc_chrdev_region(&devno,0,MAX_DEVICE,CHR_DEVICE_NAME);//$cat /proc/devices
    if (ret < 0) {
		printk(KERN_ERR DEBUG_TAG "Can't get major\n");
		return ret;
	}
    
	/* get major number and save it in major */
	major = MAJOR(devno);
    
    init_driver_context(MINOR(devno),MAX_DEVICE,&driver_contexts);
    
    dev_class = class_create(THIS_MODULE,PCI_DEVICE_NAME); //$ls /sys/class
    if(dev_class == NULL ){
		printk(KERN_ERR DEBUG_TAG "class_create failed\n");
        unregister_chrdev_region(devno, MAX_DEVICE);
        return -1;
    }
    /* register pci driver */
	ret = pci_register_driver(&pci_driver);
	if (ret < 0) {
		printk(KERN_ERR DEBUG_TAG "pci-driver: can't register pci driver\n");
        class_destroy(dev_class);
        unregister_chrdev_region(devno, MAX_DEVICE);

		return ret;
	}
    
    return NO_ERROR;
   
    
}

/*
* this function is called when the module is unloaded
*
*/
static void    __exit   module_unload(void)
{
    /*
    int i;
    // unregister pci driver  
    pci_unregister_driver(&pci_driver);
    // unregister character device 
    for(i=0;i<MAX_DEVICE;++i){
        if(pci_cdev[i].pci_dev != NULL){
            cdev_del(pci_cdev[i].cdev);
        }
    }
    
    
	// free major/minor number 
	unregister_chrdev_region(devno, MAX_DEVICE);
    */
    

    printk(KERN_ALERT  DEBUG_TAG "module_unload -> class_destroy\n");
    class_destroy(dev_class);
    
    printk(KERN_ALERT  DEBUG_TAG "module_unload -> unregister_chrdev_region\n");
    unregister_chrdev_region(devno, MAX_DEVICE);
    
    printk(KERN_ALERT  DEBUG_TAG "module_unload -> pci_unregister_driver\n");
    pci_unregister_driver(&pci_driver);
    
    printk(KERN_ALERT  DEBUG_TAG "module_unload\n");
}


module_init(module_load); 
module_exit(module_unload); 
