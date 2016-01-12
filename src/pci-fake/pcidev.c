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
#include <linux/delay.h>



#define     MAX_DEVICE      4
#define     MAX_CHIP_IN_DEVICE  16
#define     CHIP_BUFF_SIZE      256
#define 	SSX10A_PROTO_DATA_LEN   250 

#define     PCI_DEVICE_NAME     "fake-pci"      /* will show in '/proc/devices' */
#define		DEBUG_TAG          " LDD-DEBUG "

#define     NO_ERROR        0

#define SSX10A_VENDER_ID    0x0922
#define SSX10A_DEVICE_ID    0x0100



 
#pragma pack(1)
struct ssxa_protocol_t{
  u32 processor_id;
  u8  cmd;
  u8  datalen;
  u8  data[SSX10A_PROTO_DATA_LEN];
};

struct  ssxa_proto_buff_t{
    union{
        struct ssxa_protocol_t  msg;
        u8  raw_data[sizeof(struct ssxa_protocol_t)];
    }data;
};
#pragma pack()
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

static void	init_workers(struct driver_context_t* driver_context)
{
	int index;  

    for(index=0;index<MAX_CHIP_IN_DEVICE;++index){
    	driver_context->workers[index].id=index;
    	driver_context->workers[index].buff_ptr= (s8*)(driver_context->w_buffer.addr + (index* sizeof(struct ssxa_protocol_t)));
    	driver_context->workers[index].tag_ptr= (u32*)(driver_context->s_buffer.addr + (index* sizeof(u32)));
    	//FIXME:check size
    	
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

static unsigned short ssxa_checksum(const void *ptr,size_t len)
{
    const unsigned char *data ;
    unsigned short	Q[2]={0,0};
	unsigned short	S[2]={0,0};
	size_t	i=0;

	data=(const unsigned char*)ptr;
	for (i=1;i<=len;++i){
		Q[1]=Q[0]+  data[i-1];
		S[1]=S[0]+  Q[1];
		Q[0]=Q[1];
		S[0]=S[1];
	}
	return S[1];
}


static  struct ssxa_protocol_t* put_crc(struct ssxa_protocol_t* msg)
{
    unsigned short* crc;
    BUG_ON( msg->datalen > SSX10A_PROTO_DATA_LEN - 2 );
    crc=(unsigned short*)(&msg->data[msg->datalen]);
    *crc=ssxa_checksum(msg,6+msg->datalen);
    *crc=cpu_to_le16(*crc);
    return msg;
}
static  struct ssxa_protocol_t* make_rest_msg(u32 chip_index,struct ssxa_protocol_t* msg)
{
    memset(msg,0,sizeof(struct ssxa_protocol_t));
    //msg->processor_id=chip_index;
    msg->processor_id=cpu_to_le32(chip_index);
    msg->cmd=(u8)('A');
    msg->datalen=0;
    put_crc(msg);
    return msg;
}

#define TAG_WRITE 0xAAAAAAAA
#define TAG_FREE  0x00000000
#define TAG_RETURNED 0x3C3C3C3C
#define SIG_INTERRUPT 0x80
#define SIG_CLEANINTERRUPT 0x70
#define DEVICE_SIG 0x01


static void	reset_io(struct driver_context_t* driver_context)
{
	
	struct  chip_worker_t* worker;
	unsigned long index;
	char data[256];
	for(index = 0;index < MAX_CHIP_IN_DEVICE;index++)
	{
		worker = &driver_context->workers[index];
		iowrite32_rep(worker->buff_ptr,data,64);
		iowrite32(TAG_FREE,worker->tag_ptr);    
	}
	/*
	unsigned long index;
	char data[256];
	struct crypt_processor_cell_t *cell;
	memset(data,0,256);

	for(index = 0;index < DEVICE_CELL_COUNT;index++)
	{
		cell = &dev->cells[index];
		iowrite32_rep(cell->buff,data,64);
		iowrite32(TAG_FREE,cell->tag);    
	}
	*/
}
/// return bytes readed
static int	read(struct  chip_worker_t* worker,void* buff,unsigned int bytes)
{
	u32 tag;
	char* data;
	data=(char*)buff;
	
	tag=ioread32(worker->tag_ptr);
	
	printk(KERN_ALERT DEBUG_TAG "ioread32 done ,tag=%08X\n",tag);
	
	if(tag==TAG_FREE)
	{   
		printk(KERN_ALERT DEBUG_TAG "TAG_FREE device responsed \n");
		memcpy_fromio(data,worker->buff_ptr,bytes);   
		printk(KERN_ALERT DEBUG_TAG "cmd=%c,data len=%d,status code=%d \n",data[0],(int)data[1],(int)data[2]);
		/*
		retval=copy_to_user((char *)buff,(const char *)data,256);

		if(retval)
		   result = 0;
		result = 256;  
		*/  
		return 256;
	}else if(tag==TAG_RETURNED){ 
		printk(KERN_ALERT DEBUG_TAG "TAG_RETURNED device responsed \n");
		memcpy_fromio(data,worker->buff_ptr,bytes);   
		printk(KERN_ALERT DEBUG_TAG "cmd=%c,data len=%d,status code=%d \n",data[0],(int)data[1],(int)data[2]);
		
		return 256;
	} else  { 
		printk(KERN_ALERT DEBUG_TAG "device not response \n");
		return 0;
	} 
	
	
}
static void	test_io(struct driver_context_t* driver_context)
{
    printk(KERN_ALERT DEBUG_TAG "test_io \n");
	struct  ssxa_protocol_t req;
	struct  chip_worker_t* worker;
	u32 tag;
	int has_read;
	int retry;
	char data[256];

	worker=&driver_context->workers[0];
	make_rest_msg(0,&req);
	
	tag=TAG_WRITE;
	memcpy_toio(worker->buff_ptr,&req,256);
	iowrite32(tag,worker->tag_ptr); 
	outb(SIG_INTERRUPT,driver_context->dev_resource.w_port);
 
/*
  outb(SIG_INTERRUPT,dev->wport);
  retval = wait_event_interruptible_timeout(cell->waitqueue,
                                   cell->waitcond,
                                   cell->timeout);
  
  if(retval==0){
#ifdef PCI_SSCRYPT_DEBUG
      printk(KERN_ALERT"call wait_event failed\n");
#endif
      level_cell(cell);
	return -ERESTARTSYS;
  }
*/

	for(retry =0;retry<5;++retry){
		//
		
	    printk(KERN_ALERT DEBUG_TAG "start delay before read \n");
		//msleep(1000);
		mdelay(1000);
	    printk(KERN_ALERT DEBUG_TAG "delay end \n");
		has_read=read(worker,data,256);
	    printk(KERN_ALERT DEBUG_TAG "bytes read=%d \n",has_read);
	    if(has_read == 256){
	    	break;
	    }
	}
	 
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
    pci_set_drvdata(dev,this_context);
    this_context->pci_dev=dev;
    
    ret_code = hold_pci_resources(dev,this_context);
    if(ret_code != 0){
        dev_err(&(dev->dev), DEBUG_TAG "hold_pci_resources failed \n");
        return -1;
    }
    init_workers(this_context);
    dev_info(&(dev->dev), "pci probe ==> do handle \n");
    
    dev_info(&(dev->dev), "pci probe ==> do reset io \n");
    reset_io(this_context);
    dev_info(&(dev->dev), "pci probe ==> do test \n");
    test_io(this_context);
	return 0;
}

static void remove(struct pci_dev *dev)
{
	/* clean up any allocated resources and stuff here.
	 * like call release_region();
	 */
	struct driver_context_t* this_context;
	this_context= pci_get_drvdata(dev);
	

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