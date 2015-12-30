obj-m += chardevice1.o
chardevice1-objs := chardev.o file_op.o shared.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
