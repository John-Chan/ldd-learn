//////////////////////////////////////////////////////////////////
/// something shared for multi sources file
///
#ifndef     SSXA_PCI_COMMON_H
#define     SSXA_PCI_COMMON_H

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/fs.h>


/*
 *
 */
#define     MAX_DEVICE      8
#define     MAX_CHIP_IN_DEVICE  16
#define     CHIP_BUFF_SIZE      256
#define     PCI_DEVICE_NAME     "ssxa-pci"      /* will show in '/proc/devices' */
#define     CHR_DEVICE_NAME     "ssxa-cdev"      /* will show in '/proc/devices' */
#define		DEBUG_TAG          " LDD-DEBUG "
#define     BUFFER_SIZE     256
#define     NO_ERROR        0

#define PROC_FS_ENTRY_NAME  "ssxa-pci"
#define SSX10A_VENDER_ID    0x0922
#define SSX10A_DEVICE_ID    0x0100
#define SSX10A_PROTO_DATA_LEN   250
//#define SSA10A_BAR_IO       0
//#define SSA10A_BAR_MEM      1

 
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

struct  device_memory_addr_t{
    unsigned long addr;
    unsigned long size;
};
struct  device_io_addr_t{
   
    unsigned long w_port;
    unsigned long r_port;
    unsigned long c_port;
    unsigned long s_port;
};
struct  chip_worker_t{
    u8      id;
    s8*     buff_ptr; /* size : CHIP_BUFF_SIZE */
    u32*    tag_ptr;
};

struct  driver_context_t{
    int     minor;
	struct  pci_dev *pci_dev;
	struct  cdev *cdev;
    struct  device_memory_addr_t    mem_resource;
    struct  device_io_addr_t        io_resource;
    struct  chip_worker_t           workers[MAX_CHIP_IN_DEVICE];
};

#endif//!SSXA_PCI_COMMON_H

 
