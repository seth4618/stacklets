CC=gcc-4.8
CFLAGS=-g -DDEBUG -O
DEPS = myfib.h seedStack.h readyQ.h stacklet.h debug.h
OBJ = myfib.o seedStack.o readyQ.o stacklet.o debug.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fib: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

myfib:
	$(CC) $(CFLAGS) -S myfib.c

clean:
	rm -f *.o fib
