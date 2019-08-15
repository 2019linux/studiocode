#!/bin/bash
KVER := $(shell uname -r)

KDIR := /lib/modules/$(KVER)/build

PWD ?= $(shell pwd)

obj-m += scull.o 

all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean

