obj-m := batch-syscalls.o
batch-syscalls-objs := module.o log.o

ifdef DEBUG
	CFLAGS_module.o := -DDEBUG
    CFLAGS_log.o := -DDEBUG
endif

clean-files := *.o *.mod.c *.mod.o *.ko *.symvers *.o.d

all: module

.PHONY: module
module:
	${MAKE} -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

.PHONY: clean
clean:
	${MAKE} -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	$(RM) $(clean-files)

.PHONY: load
load:
	insmod batch-syscalls.ko

.PHONY: unload
unload:
	rmmod batch_syscalls

.PHONY: test
test:
	cd test && ((${MAKE} test && exit 0) || exit -1)