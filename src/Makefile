CC = @gcc

CFLAGS = -Wall -g
LIBS=-fopenmp -pthread -lm


all: gd

gd:
	$(CC) $(CFLAGS) -c gd_utils.c gd_utils.h gd_types.h $(LIBS)
	$(CC) $(CFLAGS) gd_sched.c gd_sched.h gd_types.h gd_utils.h gd_utils.o  -o gd $(LIBS)

clean:
	rm -f *.o
	rm gd
