obj-m += chardev.o 
obj-m += file_op.o
# obj-m += chardev_all.o 
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
