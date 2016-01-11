obj-m += pci-ssxa.o
pci-ssxa-objs := pcidev.o shared.o file_op.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
