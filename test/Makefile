all: test

.PHONY: test
test: generate
	gcc test.c -I../ -o test
	sudo ./test
	${MAKE} clean

.PHONY: generate
generate:
	go run generate.go

.PHONY: clean
clean:
	rm -rf test *.bin