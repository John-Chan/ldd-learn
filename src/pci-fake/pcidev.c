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
#define     MAX_CHIP_IN_DEVICE  16
#define     CHIP_BUFF_SIZE      256
#define     PCI_DEVICE_NAME     "fake-pci"      /* will show in '/proc/devices' */
#define		DEBUG_TAG          " LDD-DEBUG "

#define     NO_ERROR        0

#define SSX10A_VENDER_ID    0x0922
#define SSX10A_DEVICE_ID    0x0100


struct driver_buffer_t{
	char* addr;
	unsigned long size;
};
struct  device_io_addr_t{
   
    unsigned long w_port;
    unsigned long r_port;
    unsigned long c_port;
    unsigned long s_port;
    unsigned long mem_addr;
    unsigned long mem_size;
};
struct  chip_worker_t{
    u8      id;
    s8*     buff_ptr; /* size : CHIP_BUFF_SIZE */
    u32*    tag_ptr;
};

struct  driver_context_t{
    //int     alive;/* bool */
    int     minor;
	struct  pci_dev *pci_dev;
	struct  cdev cdev;
    struct  device_io_addr_t        dev_resource; /* unmaped memory */
    struct  chip_worker_t           workers[MAX_CHIP_IN_DEVICE];
    
    /* base_addr io_port w_buffer r_buffer save address which was maped */
    struct	driver_buffer_t			base_addr;
    //struct	driver_buffer_t			io_port;
    struct	driver_buffer_t			w_buffer;
    struct	driver_buffer_t			s_buffer;
    
    u8								irq;
};

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


/// return 0 success,others fail
static int  hold_pci_resources(struct pci_dev *pcidev,struct driver_context_t* driver_context)
{
    int ret_code;
    int bar;
    
    unsigned long reg_falgs;
    unsigned long base_port;
    unsigned long base_port_size;
 
 	//return 0;
 	
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
	//return 0;
	// FIXME: free_irq(driver_context->irq,pci_sscrypt_dev);
	iounmap(driver_context->base_addr.addr);
	pci_release_regions(driver_context->pci_dev);
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
MODULE_DESCRIPTION("FAKE PCI  driver");
MODULE_AUTHOR("john (cpp.cheen@gmail.com)");
MODULE_LICENSE("GPL");

static struct  driver_context_t   driver_contexts[MAX_DEVICE];
//static struct  class*   dev_class; //$ls /sys/class
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
    struct driver_context_t* this_context;

	ret_code=pci_enable_device(dev);
	
    if(ret_code != 0){
        dev_err(&(dev->dev), DEBUG_TAG "pci_enable_device failed \n");
        return -1;
    }

    this_context= &driver_contexts[0];
    pci_set_drvdata(pcidev,this_context);
    this_context->pci_dev=dev;
    
    ret_code = hold_pci_resources(dev,this_context);
    if(ret_code != 0){
        dev_err(&(dev->dev), DEBUG_TAG "hold_pci_resources failed \n");
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
	struct driver_context_t* this_context;
	this_context= pci_get_drvdata(pcidev);
	

	free_pci_resources(this_context);

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
    init_driver_contexts(MINOR(devno),MAX_DEVICE,driver_contexts);
	ret_code = pci_register_driver(&pci_driver);
	if (ret_code < 0) {
		printk(KERN_ERR DEBUG_TAG "pci-driver: can't register pci driver\n");
        //class_destroy(dev_class);
        unregister_chrdev_region(devno, MAX_DEVICE);

	}
	
	return ret_code;
	//return pci_register_driver(&pci_driver);
}

static void __exit krn_module_exit(void)
{
	pci_unregister_driver(&pci_driver);
    unregister_chrdev_region(devno, MAX_DEVICE);
}


module_init(krn_module_init);
module_exit(krn_module_exit);