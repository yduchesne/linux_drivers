MOD_MAME := echo
DEV_NAME := echo
obj-m := $(MOD_MAME).o

make_node:
	@sudo ../scripts/make_node.sh $(MOD_MAME)

remove_node:
	@sudo ../scripts/remove_node.sh $(MOD_MAME)


install:
	@sudo ../scripts/install_mod.sh $(MOD_MAME)

uninstall:
	@sudo ../scripts/uninstall_mod.sh $(MOD_MAME)

compile:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

all: compile remove_node uninstall install make_node
	@echo "Compiled and installed module"

clean: remove_node uninstall
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean