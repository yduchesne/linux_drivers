KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

OBJ := ramdrv

DEVNAME := blkram

obj-m := $(OBJ).o

all: run
	@make clean

run:  load 

compile:
	@$(MAKE) -C $(KDIR) M=$(PWD) modules 

load:	compile
	@echo "try \"tail -f /var/log/messages\" in another window(as root) ..";
	sudo insmod ./$(OBJ).ko
	#sync
	sudo mke2fs /dev/$(DEVNAME)
	#su -c mkdir /tmp/BDD
	#sudo su -c mount /dev/$(DEVNAME) /tmp/BDD

unload: 
	#-umount /dev/$(DEVNAME)
	-rm -fr /tmp/BDD
	-sudo su -c "rmmod $(OBJ)"

clean: unload
	#-ls -l /tmp/BDD
	-@rm -fr $(OBJ).o $(OBJ).ko $(OBJ).*.* .$(OBJ).* .tmp_versions* [mM]odule*
