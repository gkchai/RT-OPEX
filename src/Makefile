CC = @gcc

CFLAGS = -Wall -g -w
LIBS=-fopenmp -pthread -lm


all: cwarp gd

cwarp:
	$(CC) $(CFLAGS) -c ../cwarp/*.c

gd:
	$(CC) $(CFLAGS) *.c warp*.o -o gd.o $(LIBS)

clean:
	rm -f *.o
