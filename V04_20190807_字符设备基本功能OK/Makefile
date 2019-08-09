#!/bin/bash
KVER := $(shell uname -r)

KDIR := /lib/modules/$(KVER)/build

PWD ?= $(shell pwd)

obj-m += crystal_cdev.o 

all:
	make -C $(KDIR) M=$(PWD) modules
		
#make clean执行的操作是删除后缀为o的文件
clean:
	rm -rf *.mod.c *.o *.order *.ko *.mod.o *.symvers
