KVERSION=$(shell uname -r)
PWD := $(shell pwd)
all:
	make -C /lib/modules/$(KVERSION)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	rm *~


$(info Building with KERNELRELEASE = ${KERNELRELEASE})
obj-m := hamming.o
