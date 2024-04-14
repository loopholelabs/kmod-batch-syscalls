all: test

.PHONY: test
test:
	gcc test.c -I../ -o test
	sudo ./test

.PHONY: userspace
userspace:
	gcc userspace.c -I../ -o userspace
	sudo ./userspace

.PHONY: generate
generate:
	go run generate.go

.PHONY: clean
clean:
	rm -rf test userspace *.bin