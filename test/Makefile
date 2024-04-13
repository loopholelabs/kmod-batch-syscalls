all: test

.PHONY: test
test: generate
	gcc test.c -I../ -o test
	./test || true
	rm -rf test *.bin

.PHONY: generate
generate:
	go run generate.go