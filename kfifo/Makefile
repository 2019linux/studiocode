#!/bin/bash
KVER := $(shell uname -r)

KDIR := /lib/modules/$(KVER)/build

PWD ?= $(shell pwd)

obj-m += record-example.o 

all:
	make -C $(KDIR) M=$(PWD) modules
		
#make clean执行的操作是删除后缀为o的文件
clean:
	rm -r *.o *.ko .*.cmd *.order *.symvers .tmp_versions/ *.mod.c

