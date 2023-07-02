MOD_MAME := dice
DEV_NAME := dice
obj-m := $(MOD_MAME).o

install:
	@sudo ../scripts/install_mod.sh $(MOD_MAME)

uninstall:
	@sudo ../scripts/uninstall_mod.sh $(MOD_MAME)

compile:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

all: compile uninstall install
	@echo "Compiled and installed module"

clean: uninstall
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean