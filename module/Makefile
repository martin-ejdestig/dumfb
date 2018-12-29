# TODO: Put output in BUILD_DIR

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
export DEBUG ?= n

.PHONY: all clean install

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) clean

install:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) modules_install
