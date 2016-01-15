CC = @gcc

CFLAGS = -Wall -g
LIBS=-fopenmp -pthread -lm


all: gd

gd: clean
	$(CC) $(CFLAGS) -c src/gd_utils.c src/gd_utils.h src/gd_types.h $(LIBS)
	$(CC) $(CFLAGS) src/gd_sched.c src/gd_sched.h src/gd_types.h src/gd_utils.h src/gd_utils.o  -o src/gd $(LIBS)

clean:
	rm -f *.o
	rm gd
