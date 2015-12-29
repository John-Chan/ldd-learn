//////////////////////////////////////////////////////////////////
/// something shared for multi sources file
///
#ifndef     LDD_DEV_SHARED_H
#define     LDD_DEV_SHARED_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>


/*
 *
 */
#define     MAX_DEVICE      32
#define     DEVICE_NAME     "CHAR-DEV"      /* will show in '/proc/devices' */
#define     BUFFER_SIZE     32
#define     NO_ERROR        0

static int major;
static int has_open=0;
static char msg_buff[BUFFER_SIZE];
static char* msg_ptr;

#endif//!LDD_DEV_SHARED_H


