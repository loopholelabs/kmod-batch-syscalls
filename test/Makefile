all: test

.PHONY: test
test: generate
	gcc test.c -I../ -o test
	sudo ./test
	${MAKE} clean

.PHONY: test-normal
test-normal: generate
	gcc normal.c -I../ -o normal
	sudo ./normal
	${MAKE} clean

.PHONY: generate
generate:
	go run generate.go

.PHONY: clean
clean:
	rm -rf test normal *.bin