CC = @gcc

CFLAGS = -Wall -g -w
LIBS=-fopenmp -pthread -lm


all: cwarp gd gd_stat

cwarp:
	$(CC) $(CFLAGS) -c ../cwarp/*.c

gd:
	#$(CC) $(CFLAGS) *.c warp*.o -o gd.o $(LIBS)
	$(CC) $(CFLAGS) gd_proc.c  gd_sched.c gd_trans.c  gd_utils.c warp*.o -o gd.o $(LIBS)


gd_stat:
	$(CC) $(CFLAGS) gd_proc.c  gd_sched_static.c gd_trans.c  gd_utils.c warp*.o -o gd.o $(LIBS)


clean:
	rm -f *.o
