#obj-m += chardev.o 
#obj-m += file_op.o
# obj-m += chardev_all.o 
 
obj-m += chardevice.o
chardevice-objs := chardev.o file_op.o shared.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
