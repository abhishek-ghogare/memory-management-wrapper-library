CONFIG_MODULE_SIG=n
CONFIG_MODULE_SIG_ALL=n
CONFIG_DEBUG_INFO=y

EXTRA_CFLAGS := -I./

CC = gcc
SOURCES = test.c mmwl_core.c
KSOURCE = ktest.c mmwl_core.c
MY_CFLAGS += -g -DDEBUG
ccflags-y += ${MY_CFLAGS}
CC += ${MY_CFLAGS}

obj-m += ktest_module.o
ktest_module-objs += ktest.o mmwl_core.o

all: test ktest

test: $(SOURCES)
	$(CC) -g -o $@ $(SOURCES)

ktest: $(KSOURCES)
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	EXTRA_CFLAGS="$(MY_CFLAGS)"
.PHONY: clean

clean:
	rm -f test
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
