all: page_fault

ifdef VERIFY
    CFLAGS_page_fault.o := -DVERIFY
    CFLAGS_page_fault_userspace.o := -DVERIFY
endif

.PHONY: page_fault
page_fault:
	sudo sysctl -w vm.max_map_count=2097152
	gcc page_fault/page_fault.c -I../ -o page_fault
	sudo ./page_fault

.PHONY: page_fault_userspace
page_fault_userspace:
	sudo sysctl -w vm.max_map_count=2097152
	gcc page_fault/page_fault_userspace.c -I../ -o page_fault_userspace
	sudo ./page_fault_userspace

userspace: page_fault_userspace

.PHONY: generate
generate:
	go run generate.go

.PHONY: clean
clean:
	rm -rf page_fault page_fault_userspace *.bin