obj-m := batch-syscalls.o
batch-syscalls-objs := module.o log.o

ifdef DEBUG
	CFLAGS_module.o := -DDEBUG
    CFLAGS_log.o := -DDEBUG
endif

ifdef BENCHMARK
	CFLAGS_module.o := -DBENCHMARK
	CFLAGS_log.o := -DBENCHMARK
endif

clean-files := *.o *.mod.c *.mod.o *.ko *.symvers *.o.d

all: module

.PHONY: module
module:
	${MAKE} -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

.PHONY: clean
clean: test-clean
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

.PHONY: test-userspace
test-userspace:
	cd test && ((${MAKE} userspace && exit 0) || exit -1)

.PHONY: test-generate
test-generate:
	cd test && ((${MAKE} generate && exit 0) || exit -1)

.PHONY: test-clean
test-clean:
	cd test && ((${MAKE} clean && exit 0) || exit -1)