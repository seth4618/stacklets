CC=gcc
CFLAGS=-g -O3 -DSIM -I ../../../gem5-stable-629fe6e6c781/util/m5 ../../../gem5-stable-629fe6e6c781/util/m5/m5op_x86.S
LDLIBS=-lpthread
myapp: myapp.c mylock.o u_interrupt.o queue.o spinlock.o system.o atomic_xchange.o ../../pthread.o
	$(CC) $(CFLAGS) -static -o myapp myapp.c mylock.o u_interrupt.o queue.o spinlock.o system.o atomic_xchange.o ../../pthread.o $(LDLIBS)
atomic_xchange.o: atomic_xchange.S
	$(CC) $(CFLAGS) -static -c atomic_xchange.S
queue.o: queue.c
	$(CC) $(CFLAGS) -static -c queue.c
spinlock.o: spinlock.c
	$(CC) $(CFLAGS) -static -c spinlock.c
u_interrupt.o: u_interrupt.c
	$(CC) $(CFLAGS) -static -c u_interrupt.c
mylock.o: mylock.c
	$(CC) $(CFLAGS) -static -c mylock.c
system.o: system.c
	$(CC) $(CFLAGS) -static -c system.c
../pthread.o: ../../pthread.c ../../pthread_defs.h ../../tls_defs.h ../Makefile
	$(CC) $(CFLAGS) -c ../../pthread.c -o ../../pthread.o
clean:
	rm -f *.o myapp
