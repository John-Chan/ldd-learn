#include "shared.h"
#include "common.h"

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>		/* for cdev_ */
#include <asm/uaccess.h>    /* for put_user */
#include <asm/byteorder.h>

 
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


static  unsigned long g_falgs;
static  unsigned long g_base_port;
static  unsigned long g_base_port_size;
static  unsigned long g_phys_addr;
static  unsigned long g_mem_size;

static  unsigned long g_w_port;
static  unsigned long g_r_port;
static  unsigned long g_c_port;
static  unsigned long g_s_port;


struct  chip_worker{
    u8      id;
    
    //
    s8*     buff_ptr; /* size : CHIP_BUFF_SIZE */
    u32*    tag_ptr;
};

/**
 *  This structure is used to link a pci_dev to a cdev 
 *
 */
struct pci_cdev_pair {
	int minor;
	struct pci_dev *pci_dev;
	struct cdev *cdev;
    
    unsigned long phys_addr;
    unsigned long mem_size;

    unsigned long w_port;
    unsigned long r_port;
    unsigned long c_port;
    unsigned long s_port;
    struct  chip_worker     workers[MAX_CHIP_IN_DEVICE];
    
};

static struct pci_cdev_pair pci_cdev[MAX_DEVICE];
struct ssxa_protocol_t
{
  u32 processor_id;
  u8  cmd;
  u8  datalen;
  u8  data[250];
};
static  struct ssxa_protocol_t* put_crc(struct ssxa_protocol_t* msg)
{
    unsigned short* crc=(unsigned short*)(&msg->data[msg->datalen]);
    *crc=ssxa_checksum(msg,6+msg->datalen);
    *crc=cpu_to_le16(*crc);
}
static  struct ssxa_protocol_t* make_rest_msg(u32 chip_index,struct ssxa_protocol_t* msg)
{
    memset(msg,0,sizeof(ssxa_protocol_t));
    msg->processor_id=chip_index;
    msg->cmd=(u8)('A');
    msg->datalen=0;
    put_crc(msg);
    return msg;
}

static  void    test_reset_chip(unsigned char chip_index)
{
    struct ssxa_protocol_t msg;
    make_rest_msg(1,&msg);
    pci_cdev
}

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

 #if    0
 
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
		dev_err(&(dev->dev), "Can't request SSA10A_BAR_IO\n");
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
#endif
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
	pci_release_regions(dev);
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
        if(pci_cdev[i].pci_dev != NULL){
            cdev_del(pci_cdev[i].cdev);
        }
    }
    
    
	/* free major/minor number */
	unregister_chrdev_region(devno, MAX_DEVICE);
    
    printk(KERN_ALERT  DEBUG_TAG "module_unload\n");
}


module_init(module_load); 
module_exit(module_unload); 

#if     0
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

#endif

#if 0


static int pci_sscrypt_dev_probe(struct pci_dev *dev,const struct pci_device_id *id)
{
  int retval;
  int devno;
  int bar;
  unsigned long falgs;
  unsigned long base_port;
  unsigned long base_port_size;
  unsigned long phys_addr;
  unsigned long mem_size;

  struct pci_sscrypt_dev_t *pci_sscrypt_dev;

#ifdef PCI_SSCRYPT_DEBUG
  printk(KERN_DEBUG "entry pci_sscrypt_dev_probe()\n");
#endif
  retval =  pci_enable_device(dev);
  if( retval)
    {
#ifdef PCI_SSCRYPT_DEBUG
     printk(KERN_ERR"pci_enable_device falied %u\n",retval);
#endif
      return retval;
    }

  retval = pci_request_regions(dev,DEVICE_NAME);
  if ( retval )
  {
#ifdef PCI_SSCRYPT_DEBUG
    printk(KERN_ERR"call pci_request_regions() falied\n");
#endif
    return retval;
  }
  phys_addr = 0 ;
  mem_size = 0; 
  retval = 0 ;
  pci_sscrypt_dev = pci_sscrypt_dev_allocate(dev);
  for ( bar = 0; bar < 6; bar ++ )
  {
      falgs = pci_resource_flags(dev,bar);
      if ( falgs&IORESOURCE_IO )
      {
        base_port =  pci_resource_start(dev,bar);
        base_port_size = pci_resource_len(dev,bar);   
        pci_sscrypt_dev->wport = base_port;
        pci_sscrypt_dev->sport = base_port  + 1;
        pci_sscrypt_dev->cport = base_port  + 2;
        pci_sscrypt_dev->rport = base_port  + 3;    
#ifdef PCI_SSCRYPT_DEBUG
        printk(KERN_INFO"write data complete signal port %d\n",pci_sscrypt_dev->wport);
        printk(KERN_INFO"process complete signal port %d\n",pci_sscrypt_dev->sport);
        printk(KERN_INFO"clear interrupt signal port %d\n",pci_sscrypt_dev->cport);
        printk(KERN_INFO"reset device port %d\n",pci_sscrypt_dev->rport);
#endif
      }
      else if ( falgs&IORESOURCE_MEM )
      {
         phys_addr = pci_resource_start(dev,bar);
         mem_size  = pci_resource_len(dev,bar);
      }
  }
  
  pci_sscrypt_dev->irq = dev->irq;
  pci_sscrypt_dev->base_map_address =  ioremap(phys_addr,mem_size);
  pci_sscrypt_dev->wmapaddr = pci_sscrypt_dev->base_map_address;
  memset_io(pci_sscrypt_dev->base_map_address,0,mem_size);
  pci_sscrypt_dev->smapaddr = pci_sscrypt_dev->wmapaddr+4096; 
#ifdef PCI_SSCRYPT_DEBUG
  printk(KERN_INFO"device irq %d\n", dev->irq);
  printk(KERN_INFO"base_map_address %p\n",pci_sscrypt_dev->base_map_address);
  printk(KERN_INFO"device memory size %lu\n",mem_size);
#endif
  tasklet_init(&pci_sscrypt_dev->tasklet,
               pci_do_tasklet,
               (unsigned long)pci_sscrypt_dev);
  devno = MKDEV(pci_sscrypt_dev_major,pci_sscrypt_dev_curminor++);
  pci_sscrypt_dev_init(pci_sscrypt_dev,devno);
#if LINUX_VERSION_CODE==LINUX_2_1_16
  class_device_create(pci_sscrypt_class, NULL, devno,NULL, "pci_sscrypt%d", pci_sscrypt_dev_curdevno++);
#else
  device_create(pci_sscrypt_class,NULL , devno,NULL, "pci_sscrypt%d", pci_sscrypt_dev_curdevno++);
#endif

  retval = request_irq(pci_sscrypt_dev->irq ,
              pci_sscrypt_dev_interrupt,
#if LINUX_VERSION_CODE==LINUX_2_1_16
              SA_SHIRQ,
#else
              IRQF_SHARED,
#endif
              DEVICE_NAME,
              pci_sscrypt_dev
             );
  if ( retval )
  {
#ifdef PCI_SSCRYPT_DEBUG
    printk(KERN_ERR"request_irq() falied %u\n",retval);
#endif
  }
#ifdef PCI_SSCRYPT_DEBUG
  printk(KERN_DEBUG "level pci_sscrypt_dev_probe()\n");
#endif
  return retval;
  
}
#endif