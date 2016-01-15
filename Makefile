CC = @gcc

CFLAGS = -Wall -g
LIBS=-fopenmp -pthread -lm

all: rx_sched.o wrapper.o
	$(CC) $(CFLAGS) wrapper.o rx_sched.o -o rx_sched $(LIBS)
	# $(CC) $(CFLAGS) test_deadline.c -o test.o $(LIBS)

rx_sched.o: rx_sched.c wrapper.h
	$(CC) $(CFLAGS) -c rx_sched.c $(LIBS)


wrapper.o: wrapper.c wrapper.h
	$(CC) $(CFLAGS) -c wrapper.c $(LIBS)

gd: clean
	$(CC) $(CFLAGS) -c gd_utils.c gd_utils.h gd_types.h $(LIBS)
	$(CC) $(CFLAGS) gd_sched.c gd_sched.h gd_types.h gd_utils.h gd_utils.o  -o gd $(LIBS)

clean:
	rm -f *.o
	rm gd
