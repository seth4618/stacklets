CC=gcc
CFLAGS=-g -static -O3 -I ../simulation/gem5-stable-629fe6e6c781/util/m5 ../simulation/gem5-stable-629fe6e6c781/util/m5/m5op_x86.S
LDLIBS=-lpthread
myapp: myapp.c mylock.o u_interrupt.o queue.o spinlock.o system.o atomic_xchange.o
	$(CC) $(CFLAGS) -o myapp myapp.c mylock.o u_interrupt.o queue.o spinlock.o system.o atomic_xchange.o $(LDLIBS)
atomic_xchange.o: atomic_xchange.S
	$(CC) $(CFLAGS) -c atomic_xchange.S
queue.o: queue.c
	$(CC) $(CFLAGS) -c queue.c
spinlock.o: spinlock.c
	$(CC) $(CFLAGS) -c spinlock.c
u_interrupt.o: u_interrupt.c
	$(CC) $(CFLAGS) -c u_interrupt.c
mylock.o: mylock.c
	$(CC) $(CFLAGS) -c mylock.c
system.o: system.c
	$(CC) $(CFLAGS) -c system.c
clean:
	rm -f *.o myapp
