CFLAGS = gcc -Wall

build: httpproxy.c
	$(CFLAGS) httpproxy.c -o httpproxy

.PHONY: clean

clean:
	rm -f httpproxy
	rm -f file_*.txt
