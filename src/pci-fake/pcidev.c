#include <linux/err.h>
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




#define     MAX_DEVICE      4
#define     PCI_DEVICE_NAME     "fake-pci"      /* will show in '/proc/devices' */
#define		DEBUG_TAG          " LDD-DEBUG "

#define     NO_ERROR        0

#define SSX10A_VENDER_ID    0x0922
#define SSX10A_DEVICE_ID    0x0100

s

MODULE_DESCRIPTION("FAKE PCI  driver");
MODULE_AUTHOR("john (cpp.cheen@gmail.com)");
MODULE_LICENSE("GPL");


static dev_t devno;

static struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(SSX10A_VENDER_ID, SSX10A_DEVICE_ID), },
	{ 0, }
};

/**
 * This macro ...
 *
 */
MODULE_DEVICE_TABLE(pci, pci_ids);


static int probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int ret_code;

	ret_code=pci_enable_device(dev);
	
    if(ret_code != 0){
        dev_err(&(dev->dev), DEBUG_TAG "pci_enable_device failed \n");
        return -1;
    }

    dev_info(&(dev->dev), "pci probe ==> do handle \n");
	return 0;
}

static void remove(struct pci_dev *dev)
{
	/* clean up any allocated resources and stuff here.
	 * like call release_region();
	 */
    dev_info(&(dev->dev), DEBUG_TAG "pci remove \n"); 
}

static struct pci_driver pci_driver = {
	.name = "fake-pci-driver",
	.id_table = pci_ids,
	.probe = probe,
	.remove = remove,
};

static int __init krn_module_init(void)
{
	int ret_code;
    ret_code= alloc_chrdev_region(&devno,0,MAX_DEVICE,PCI_DEVICE_NAME);//$cat /proc/devices
    if (ret_code < 0) {
		printk(KERN_ERR DEBUG_TAG "can't get major\n");
		return ret_code;
	}
	ret_code = pci_register_driver(&pci_driver);
	if (ret_code < 0) {
		printk(KERN_ERR DEBUG_TAG "pci-driver: can't register pci driver\n");
        //class_destroy(dev_class);
        unregister_chrdev_region(devno, MAX_DEVICE);

	}
	return ret_code
	//return pci_register_driver(&pci_driver);
}

static void __exit krn_module_exit(void)
{
	pci_unregister_driver(&pci_driver);
    unregister_chrdev_region(devno, MAX_DEVICE);
}


module_init(krn_module_init);
module_exit(krn_module_exit);