all: test

.PHONY: test
test:
	sudo sysctl -w vm.max_map_count=2097152
	gcc test.c -I../ -o test
	sudo ./test

.PHONY: userspace
userspace:
	sudo sysctl -w vm.max_map_count=2097152
	gcc userspace.c -I../ -o userspace
	sudo ./userspace

.PHONY: generate
generate:
	go run generate.go

.PHONY: clean
clean:
	rm -rf test userspace *.bin