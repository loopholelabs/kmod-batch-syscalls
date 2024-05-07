obj-m := batch-syscalls.o
batch-syscalls-objs := module.o log.o

ifdef DEBUG
	CFLAGS_module.o += -DDEBUG
	CFLAGS_log.o += -DDEBUG
endif

ifdef BENCHMARK
	CFLAGS_module.o += -DBENCHMARK
	CFLAGS_log.o += -DBENCHMARK
endif

clean-files := *.o *.mod.c *.mod.o *.ko *.symvers *.o.d

all: module

.PHONY: module
module:
	${MAKE} -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

.PHONY: clean
clean: tests-clean
	${MAKE} -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	$(RM) $(clean-files)

.PHONY: load
load:
	insmod batch-syscalls.ko

.PHONY: unload
unload:
	rmmod batch_syscalls

.PHONY: tests
tests:
	cd tests && ((${MAKE} all && exit 0) || exit -1)

.PHONY: tests-userspace
tests-userspace:
	cd tests && ((${MAKE} userspace && exit 0) || exit -1)

.PHONY: tests-generate
tests-generate:
	cd tests && ((${MAKE} generate && exit 0) || exit -1)

.PHONY: tests-clean
tests-clean:
	cd tests && ((${MAKE} clean && exit 0) || exit -1)
