all: page_fault page_fault_multithread

ifdef VERIFY
    CFLAGS += -DVERIFY
endif

.PHONY: page_fault
page_fault:
	sudo sysctl -w vm.max_map_count=8388608
	gcc ${CFLAGS} page_fault/page_fault.c -I../ -o page_fault.out
	sudo ./page_fault.out

.PHONY: page_fault_multithread
page_fault_multithread:
	sudo sysctl -w vm.max_map_count=8388608
	gcc ${CFLAGS} page_fault/page_fault_multithread.c -I../ -o page_fault_multithread.out
	sudo ./page_fault_multithread.out

.PHONY: page_fault_userspace
page_fault_userspace:
	sudo sysctl -w vm.max_map_count=8388608
	gcc ${CFLAGS} page_fault/page_fault_userspace.c -I../ -o page_fault_userspace.out
	sudo ./page_fault_userspace.out

userspace: page_fault_userspace

.PHONY: generate
generate:
	go run generate.go

.PHONY: clean
clean:
	rm -rf *.out *.bin
