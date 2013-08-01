CC = gcc
CFLAGS = -O2 -Wall -I ..

all: adder post

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c
post: post.c 
	$(CC) $(CFLAGS) -o post post.c

clean:
	rm -f adder post *~
