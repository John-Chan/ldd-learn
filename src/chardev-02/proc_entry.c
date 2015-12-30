#include "proc_entry.h"
#include "shared.h"
#include "common.h"

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
 

static  int     poc_fop_show(struct seq_file *seqf,void* p)
{
    seq_printf(seqf,"device-name=%s major=%d open-counter=%d buff-size=%d\n",
            DEVICE_NAME,major,has_open,BUFFER_SIZE
            );
    return 0;
}
static  int     poc_fop_open(struct inode* node,struct file* filep)
{
    return single_open(filep,poc_fop_show,NULL);
}

 static const struct file_operations poc_fops={
     .owner=THIS_MODULE,
     .open=poc_fop_open,
     .read=seq_read,
     .llseek=seq_lseek,
     .release=single_release,
 };

static struct proc_dir_entry* poc_entry;

int     poc_entry_open(const char* name)
{
    poc_entry=proc_create(name,0,NULL,&poc_fops);
    if(NULL == poc_entry){
        return -ENOMEM;
    }
    return NO_ERROR;
}
void    poc_entry_close(const char* name)
{
    remove_proc_entry(name,NULL);
}