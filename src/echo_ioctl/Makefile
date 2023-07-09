MOD_MAME := echo
APP_NAME := echo_client
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
	gcc -o $(APP_NAME) $(APP_NAME).c

	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

all: compile remove_node uninstall install make_node
	@echo "Compiled and installed module"

clean: remove_node uninstall
	@ if [ -f "$(APP_NAME)" ]; then rm "$(APP_NAME)"; fi;
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
