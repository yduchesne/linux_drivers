KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

OBJ := main

obj-m := $(OBJ).o

all: run
	@make clean

run:  load 

compile:
	@$(MAKE) -C $(KDIR) M=$(PWD) modules 

load:	compile
	@echo "try \"tail -f /var/log/messages\" in another window(as root) ..";
	sudo su -c "insmod ./$(OBJ).ko"
	sync;
	sudo su -c mke2fs /dev/sbd
	su -c mkdir /tmp/BDD
	sudo su -c mount /dev/sbd /tmp/BDD
	df -k /tmp/BDD
	ls -l /tmp/BDD
	cp /etc/hosts /tmp/BDD/
	ls -l /tmp/BDD

unload: 
	-umount /dev/sbd
	-rm -fr /tmp/BDD
	-sudo su -c "rmmod $(OBJ)"

clean: unload
	#-ls -l /tmp/BDD
	-@rm -fr $(OBJ).o $(OBJ).ko $(OBJ).*.* .$(OBJ).* .tmp_versions* [mM]odule*
