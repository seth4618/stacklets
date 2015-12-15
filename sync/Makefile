CC=gcc
CFLAGS=-g -static -O3 -I ../simulation/gem5-stable-629fe6e6c781/util/m5 
LDLIBS=-lpthread

myapp: myapp.o mylock.o u_interrupt.o queue.o spinlock.o atomic_xchange.o debug.o myassert.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# pull in dependency info for *existing* .o files
-include $(addprefix .,$(OBJ:%.o=%.d))

%.o: %.c
	$(CC) -c -o $*.o $*.c $(CFLAGS)
	gcc -MM $(CFLAGS) $*.c > .$*.d

clean:
	rm -f *.o myapp .*.d

