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
    int i;
    int j;
    BUG_ON(count > MAX_DEVICE); 
    memset(drv_ctx_head,0,sizeof(struct driver_context_t) * count );
 
    for(i=0;i<count;++i){
        drv_ctx_head[i].minor=first_minor++;
        for(j=0;j<MAX_CHIP_IN_DEVICE;++j){
            drv_ctx_head[i].workers[j].id = j;
        }
    }
}

static void clear_driver_context(struct driver_context_t* drv_ctx_head,int count)
{
    //
    BUG_ON(count > MAX_DEVICE); 
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

static dev_t ssxa_devno;
static int major;
static struct  driver_context_t   driver_contexts[MAX_DEVICE];
static struct  class*   dev_class; //$ls /sys/class
//static struct  device*	dev_device;//$ls /dev/ 

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

/// return 0 success,others fail
static int  hold_pci_resources(struct pci_dev *pcidev,struct driver_context_t* driver_context)
{
    int ret_code;
    int bar;
    
    unsigned long reg_falgs;
    unsigned long base_port;
    unsigned long base_port_size;
 
    ret_code=pci_request_regions(pcidev,PCI_DEVICE_NAME);
    if(ret_code != 0){
        dev_err(&(pcidev->dev), DEBUG_TAG "pci_request_regions failed \n");
        goto fali_clean0;
    }
    
    

    for ( bar = 0; bar < 6; bar ++ )
    {
        reg_falgs = pci_resource_flags(pcidev,bar);
        dev_info(&(pcidev->dev), "looking up BAR %d,flags = %08X\n",bar,reg_falgs);
        if ( reg_falgs&IORESOURCE_IO )
        {
            base_port =  pci_resource_start(pcidev,bar);
            base_port_size = pci_resource_len(pcidev,bar);   
 
            driver_context->dev_resource.w_port = base_port;
            driver_context->dev_resource.s_port = base_port  + 1;
            driver_context->dev_resource.c_port = base_port  + 2;
            driver_context->dev_resource.r_port = base_port  + 3;  


            printk(KERN_ALERT DEBUG_TAG "found IORESOURCE_IO ,base_port=%08X,base_port_size=%08d(%08X)\n",base_port,base_port_size,base_port_size);
            printk(KERN_ALERT DEBUG_TAG "write data complete signal port %08X\n",driver_context->dev_resource.w_port);
            printk(KERN_ALERT DEBUG_TAG "process complete signal port %08X\n",driver_context->dev_resource.s_port);
            printk(KERN_ALERT DEBUG_TAG "clear interrupt signal port %08X\n",driver_context->dev_resource.c_port);
            printk(KERN_ALERT DEBUG_TAG "reset device port %08X\n",driver_context->dev_resource.r_port);
        }
        else if ( reg_falgs&IORESOURCE_MEM )
        {
            printk(KERN_ALERT DEBUG_TAG "found IORESOURCE_MEM \n");
            
            driver_context->dev_resource.mem_addr=pci_resource_start(pcidev,bar);
            driver_context->dev_resource.mem_size=pci_resource_len(pcidev,bar);
            printk(KERN_ALERT DEBUG_TAG "g_phys_addr %08X\n",driver_context->dev_resource.mem_addr);
            printk(KERN_ALERT DEBUG_TAG "g_mem_size %08d(%08X)\n",driver_context->dev_resource.mem_size,driver_context->dev_resource.mem_size);
        }
    }
	driver_context->irq = pcidev->irq;
  	driver_context->base_addr.addr=(char*)ioremap(driver_context->dev_resource.mem_addr,driver_context->dev_resource.mem_size);
  	driver_context->base_addr.size=driver_context->dev_resource.mem_size;
  	
  	if(driver_context->base_addr.addr == NULL)
  	{	
  		printk(KERN_ALERT DEBUG_TAG "ioremap failed \n");
  		goto fali_clean1;
  	}
  	
  	driver_context->w_buffer.addr=driver_context->base_addr.addr;
  	driver_context->w_buffer.size=4096;
  	// FIXME: check size
  	
  	driver_context->s_buffer.addr=driver_context->base_addr.addr + 4096;
  	driver_context->s_buffer.size=driver_context->base_addr.size - 4096;
  	// FIXME: check size
  	
	printk(KERN_ALERT DEBUG_TAG "memory mapping done ,addr=%08X,size=%08d(%08X)\n",
		driver_context->base_addr.addr,driver_context->base_addr.size,driver_context->base_addr.size);
		
	memset_io(driver_context->base_addr.addr,0x0,driver_context->base_addr.size);// clear up
#if 0
  	
	/// FIXME: setup IRQ
  ret_code = request_irq(driver_context->irq ,
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
#endif

	return 0;
fali_clean1: 
    pci_release_regions(pcidev);
fali_clean0: 
    return ret_code;   
}

static int  free_pci_resources(struct driver_context_t* driver_context)
{
	// FIXME: free_irq(driver_context->irq,pci_sscrypt_dev);
	iounmap(driver_context->base_addr.addr);
	pci_release_regions(driver_context->pci_dev);
}
// return 0 success,others fialed
static int	mount_chr_dev(struct driver_context_t* driver_context,int dev_no)
{
    int ret_code;
    int index;
    cdev_init(&driver_context->cdev,&cdev_ops);
    ret_code=cdev_add(&driver_context->cdev,dev_no,1);
    if(ret_code != 0){
  		printk(KERN_ALERT DEBUG_TAG "cdev_add failed ,code=%d \n",ret_code);
  		goto fali_clean0;
    }

    for(index=0;index<MAX_CHIP_IN_DEVICE;++index){
    	driver_context->workers[index].id=index;
    	driver_context->workers[index].buff_ptr= (s8*)(driver_context->w_buffer.addr + (index* sizeof(struct ssxa_protocol_t)));
    	driver_context->workers[index].tag_ptr= (u32*)(driver_context->s_buffer.addr + (index* sizeof(u32)));
    	//FIXME:check size
    	
    }
    
    device_create(dev_class,NULL,MKDEV(major,driver_context->minor),NULL,PCI_DEVICE_NAME);//$ls /dev/ 
    printk(KERN_ERR DEBUG_TAG "device_create done,use [ls /dev | grep %s ] to check it out\n",PCI_DEVICE_NAME);
    return 0;
fali_clean0: 
    return ret_code;   
}
static void	unmount_chr_dev(struct driver_context_t* driver_context)
{
	device_destroy(dev_class,MKDEV(major,driver_context->minor));
	cdev_del(&driver_context->cdev);
}
static int dev_attach_counter=0;
/**
 * This function is called when a new pci device is associated with a driver
 *
 * return: 0 => this driver don't handle this device
 *         1 => this driver handle this device
 *
 */
 
static int pci_driver_attach(struct pci_dev *pcidev, const struct pci_device_id *id)
{
/*
1   启用设备    pci_enable_device
2   创建自定义上下文    pci_set_drvdata
3   请求保留设备资源    pci_request_regions
4   获取硬件配置信息    pci_resource_fags
5   映射内存    ioremap,memset_io
6   初始化字符设备 cdev_init,cdev_add
7   创建设备节点  device_create
8   注册中断回调函数    request_irq
*/

    
    int ret_code;
    
    dev_info(&(pcidev->dev), DEBUG_TAG "dev_attach_counter =%d \n",dev_attach_counter);
    if(dev_attach_counter > MAX_DEVICE ){
        dev_err(&(pcidev->dev), DEBUG_TAG "so many device attached :%d \n",dev_attach_counter);
        goto fali_clean0;
    }
    ++dev_attach_counter ;
    
    ret_code=pci_enable_device(pcidev);
    if(ret_code != 0){
        dev_err(&(pcidev->dev), DEBUG_TAG "pci_enable_device failed \n");
        goto fali_clean1;
    }
    
    struct driver_context_t* this_context= &driver_contexts[dev_attach_counter - 1];
    pci_set_drvdata(pcidev,this_context);
    this_context->pci_dev=pcidev;
    
    ret_code = hold_pci_resources(pcidev,this_context);
    if(ret_code != 0){
        dev_err(&(pcidev->dev), DEBUG_TAG "hold_pci_resources failed \n");
        goto fali_clean2;
    }
    
    ret_code=mount_chr_dev(this_context,MKDEV(major,this_context->minor));
    if(ret_code != 0){
        dev_err(&(pcidev->dev), DEBUG_TAG "mount_chr_dev failed \n");
        goto fali_clean3;
    }

    dev_info(&(pcidev->dev), "pci_probe ==> do handle \n");
    return 1;
    
fali_clean4:
	unmount_chr_dev(this_context);
fali_clean3:
	free_pci_resources(this_context);
fali_clean2:
    pci_disable_device(pcidev);
fali_clean1:
    dev_attach_counter--;
fali_clean0:
    dev_info(&(pcidev->dev), DEBUG_TAG "pci_probe ==> do not handle \n"); 
    return 0;    
}

/**
 * This function is called when the driver is removed
 *
 */
static void pci_driver_detach(struct pci_dev *pcidev)
{
	struct driver_context_t* this_context;
	this_context= pci_get_drvdata(pcidev);
	
	unmount_chr_dev(this_context);

	free_pci_resources(this_context);

    pci_disable_device(pcidev);

    dev_attach_counter--;

    dev_info(&(pcidev->dev), DEBUG_TAG "pci_driver_detach \n"); 
#if 0
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
#endif
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
    int ret;
    printk(KERN_ALERT DEBUG_TAG "module_load\n");
    
    // allocate (several) major number ,start from 0
    // name will display in /proc/devices
    ret= alloc_chrdev_region(&ssxa_devno,0,MAX_DEVICE,CHR_DEVICE_NAME);//$cat /proc/devices
    if (ret < 0) {
		printk(KERN_ERR DEBUG_TAG "Can't get major\n");
		return ret;
	}
    
	/* get major number and save it in major */
	major = MAJOR(ssxa_devno);
    
    init_driver_contexts(MINOR(ssxa_devno),MAX_DEVICE,driver_contexts);
    
    dev_class = class_create(THIS_MODULE,PCI_DEVICE_NAME); //$ls /sys/class
    if(dev_class == NULL ){
		printk(KERN_ERR DEBUG_TAG "class_create failed\n");
        unregister_chrdev_region(ssxa_devno, MAX_DEVICE);
        return -1;
    }
    
    printk(KERN_ERR DEBUG_TAG "class_create done,use [ls /sys/class | grep %s ] to check it out\n",PCI_DEVICE_NAME);
    /* register pci driver */
	ret = pci_register_driver(&pci_driver);
	if (ret < 0) {
		printk(KERN_ERR DEBUG_TAG "pci-driver: can't register pci driver\n");
        class_destroy(dev_class);
        unregister_chrdev_region(ssxa_devno, MAX_DEVICE);

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

    printk(KERN_ALERT  DEBUG_TAG "module_unload -> class_destroy\n");
    class_destroy(dev_class);
    
    printk(KERN_ALERT  DEBUG_TAG "module_unload -> unregister_chrdev_region\n");
    unregister_chrdev_region(ssxa_devno, MAX_DEVICE);
    
    printk(KERN_ALERT  DEBUG_TAG "module_unload -> pci_unregister_driver\n");
    pci_unregister_driver(&pci_driver);
    
    printk(KERN_ALERT  DEBUG_TAG "module_unload\n");
}


module_init(module_load); 
module_exit(module_unload); 
