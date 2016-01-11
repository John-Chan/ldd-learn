#ifndef     SSX_PCI_UTILITYS_H
#define     SSX_PCI_UTILITYS_H

#include "common.h"
#include <linux/types.h>
#include <asm-generic/bug.h>

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
    msg->processor_id=chip_index;
    msg->cmd=(u8)('A');
    msg->datalen=0;
    put_crc(msg);
    return msg;
}

#endif//!SSX_PCI_UTILITYS_H

 
