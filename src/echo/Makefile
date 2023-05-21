MOD_MAME := echo
DEV_NAME := echo
obj-m := $(MOD_MAME).o

dev_rm:
	@major=$(awk "\\$$2= =\"$(MOD_MAME)\" {print \\$$1}" /proc/devices); \
	mknod /dev/$(DEV_NAME)0 $$major 0
	

install:
	@echo "Installing module $(MOD_MAME)" 
	@sudo insmod ./$(MOD_MAME).ko

uninstall:
	@echo "Uninstalling module $(MOD_MAME)"
	- @sudo rmmod $(MOD_MAME)
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean: uninstall
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean