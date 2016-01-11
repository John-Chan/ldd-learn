
#include"base.h"


// 该函数根据tasklet特性只能串行执行，并且只能
// 在一个CPU上执行，所以对于同一个tasklet可不考虑
// 资源竞争问题
void pci_do_tasklet(unsigned long arg)
{
 struct pci_sscrypt_dev_t *dev;
 int index;

#ifdef PCI_SSCRYPT_DEBUG
    printk(KERN_DEBUG "entry pci_do_tasklet()\n");
#endif

 dev = (struct pci_sscrypt_dev_t *)arg;
 for ( index = 0; index < DEVICE_CELL_COUNT;index++)
 {
   // 对于一个cell的读写操作可能发生在不同的线程中，所以对于一个
  // cell必须加锁，防止资源竞争
 //  down(&dev->cells[index].sem);
  if ( ioread32(dev->cells[index].tag) == TAG_RETURNED )
   {  
#ifdef PCI_SSCRYPT_DEBUG
     printk(KERN_DEBUG "processor complete request %d\n",index);
#endif
     iowrite32(TAG_FREE,dev->cells[index].tag);
     dev->cells[index].waitcond = 1;
       // 唤醒阻塞的IO调用     
    // up(&dev->cells[index].sem);
     wake_up_interruptible(&dev->cells[index].waitqueue);
    
   }
 }
#ifdef PCI_SSCRYPT_DEBUG
     printk(KERN_DEBUG "level pci_do_tasklet()\n");
#endif

}
#if LINUX_VERSION_CODE==LINUX_2_1_16
irqreturn_t pci_sscrypt_dev_interrupt(int irq,
                                      void *dev_id,
                                      struct pt_regs *regs)
#else
irqreturn_t pci_sscrypt_dev_interrupt(int irq,
                                      void *dev_id)
#endif
{
 struct pci_sscrypt_dev_t *dev = (struct pci_sscrypt_dev_t *)dev_id;
 
 // 检查设备标志，判断是否是该设备发出的中断请求
 if(!inb(dev->sport))
   return IRQ_NONE;

 // 设置清除中断请求标志端口，该步骤必须在中断例程中执行
 // 否则硬件将连续请求中断，从而造成系统进入挂起状态
 outb(SIG_INTERRUPT,dev->cport);
 // 将其它任务延迟执行，放入中断下半部分执行
 tasklet_schedule(&dev->tasklet);
 return  IRQ_HANDLED;
}