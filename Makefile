CC=gcc
CFLAGS= -std=c99 -pipe 
LDFLAGS=-lm -w

all: simulator
	$(CC) $(CFLAGS) sim.o -o sim $(LDFLAGS)

simulator: sim.c
	$(CC) $(CFLAGS) -c sim.c -lm $(LDFLAGS)

clean:
	rm *.o sim
