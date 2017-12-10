CC=gcc
CFLAGS= -std=c99 -pipe 
LDFLAGS=-lm -w

all: cachesim
	$(CC) $(CFLAGS) cachesim.o -o cachesim $(LDFLAGS)

cachesim: cachesim.c
	$(CC) $(CFLAGS) -c cachesim.c -lm $(LDFLAGS)

clean:
	rm *.o cachesim
