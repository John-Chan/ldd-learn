obj-m += chardevice2.o
chardevice2-objs := chardev.o file_op.o shared.o proc_entry.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
