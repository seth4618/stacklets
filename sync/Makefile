CC=gcc
LDLIBS=-lpthread
myapp: myapp.c mylock.o u_interrupt.o queue.o spinlock.o atomic_xchange.o
	$(CC) -o myapp myapp.c mylock.o u_interrupt.o queue.o spinlock.o atomic_xchange.o $(LDLIBS)
atomic_xchange.o: atomic_xchange.S
	$(CC) -c atomic_xchange.S
queue.o: queue.c
	$(CC) -c queue.c
spinlock.o: spinlock.c
	$(CC) -c spinlock.c
u_interrupt.o: u_interrupt.c
	$(CC) -c u_interrupt.c
mylock.o: mylock.c
	$(CC) -c mylock.c
clean:
	rm -f *.o myapp
